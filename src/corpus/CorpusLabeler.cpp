#include "CorpusLabeler.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "first/First1.h"
#include "first/FirstK.h"
#include "first/Follow1.h"
#include "first/FollowK.h"
#include "grammar/GrammarAnalysis.h"
#include "grammar/GrammarBuilder.h"
#include "grammar/GrammarSyntax.h"
#include "grammar/YieldLength.h"
#include "lr/LALRkDfa.h"
#include "lr/LRkDfa.h"
#include "lr/ParseTable.h"
#include "util/util.h"

namespace zbik {
namespace {

using Clock = std::chrono::steady_clock;

bool isSeparator(const std::string &line) {
    const std::string text = trim(line);
    return grammar_syntax::isBlankLine(line) || text == "---" || text.starts_with("===");
}

bool hasRule(const std::vector<std::string> &block) {
    return std::ranges::any_of(block, [](const std::string &line) {
        return !grammar_syntax::isBlankLine(line)
                && !grammar_syntax::isCommentLine(line);
    });
}

std::string cleanComputedLabels(const std::string &comment) {
    static const std::regex computedLabel(
            R"(\[(?:(?:notLALR|LALR|notLR|LR)(?:\([^\]]+\))?|ambig)\][ \t]*)");
    return std::regex_replace(comment, computedLabel, "");
}

std::size_t labelInsertionPosition(const std::string &comment) {
    static const std::regex preservedPrefix(
            R"(^([ \t]*;[ \t]*(?:(?:\[(?:notLL|LL)\([^\]]+\)\]|\[SLR\])[ \t]*)+))");
    std::smatch match;
    if (std::regex_search(comment, match, preservedPrefix)) {
        return match.length(1);
    }
    const std::size_t semicolon = comment.find(';');
    return semicolon == std::string::npos ? 0 : semicolon + 1;
}

std::string joinLabels(const std::vector<std::string> &labels) {
    std::string result;
    for (const std::string &label: labels) {
        if (!result.empty()) result += ' ';
        result += label;
    }
    return result;
}

void installLabels(
        std::vector<std::string> &block, const std::vector<std::string> &labels) {
    for (std::string &line: block) {
        if (grammar_syntax::isCommentLine(line)) {
            line = cleanComputedLabels(line);
        }
    }

    auto comment = std::ranges::find_if(block, [](const std::string &line) {
        return grammar_syntax::isCommentLine(line);
    });
    if (comment == block.end()) {
        block.insert(block.begin(), ";" + joinLabels(labels));
        return;
    }

    const std::size_t position = labelInsertionPosition(*comment);
    const std::string joined = joinLabels(labels);
    const bool spaceBefore = position > 0
            && (*comment)[position - 1] != ';'
            && !std::isspace(static_cast<unsigned char>((*comment)[position - 1]));
    const bool spaceAfter = position < comment->size()
            && !std::isspace(static_cast<unsigned char>((*comment)[position]));
    comment->insert(position,
            (spaceBefore ? " " : "") + joined + (spaceAfter ? " " : ""));
}

template<typename Function>
void timedStage(
        CorpusGrammarReport &report, const CorpusProgress &progress,
        std::string stage, Function &&function) {
    if (progress) progress(report.index, stage);
    const auto started = Clock::now();
    try {
        std::forward<Function>(function)();
    } catch (...) {
        report.timings.push_back({std::move(stage), Clock::now() - started});
        throw;
    }
    report.timings.push_back({std::move(stage), Clock::now() - started});
}

void runAnalyses(
        const Grammar &grammar, CorpusGrammarReport &report,
        const CorpusProgress &progress) {
    timedStage(report, progress, "analyses", [&] {
        const GrammarAnalysis structure(grammar);
        const MinYieldLength minimum(grammar);
        const MaxYieldLength maximum(grammar);
        const First1Analysis first1(grammar);
        const Follow1Analysis follow1(grammar, first1);
        static_cast<void>(structure);
        static_cast<void>(minimum);
        static_cast<void>(maximum);
        static_cast<void>(follow1);
    });
}

CorpusLookaheadStats lookaheadStats(
        const Grammar &grammar, const FirstKAnalysis &first,
        const FollowKAnalysis &follow) {
    CorpusLookaheadStats result{};
    result.k = first.maxLength();
    result.nonterminals.reserve(grammar.nonterminalCount());
    for (std::size_t index = 0; index < grammar.nonterminalCount(); ++index) {
        const NonterminalId nonterminal{static_cast<std::uint32_t>(index)};
        const std::size_t firstWords = first.first(nonterminal).size();
        const std::size_t followWords = follow.follow(nonterminal).size();
        result.firstWords += firstWords;
        result.followWords += followWords;
        result.maxFirstWords = std::max(result.maxFirstWords, firstWords);
        result.maxFollowWords = std::max(result.maxFollowWords, followWords);
        result.nonterminals.push_back({
                grammar.nonterminalName(nonterminal), firstWords, followWords});
    }
    return result;
}

void addAutomatonStats(
        CorpusGrammarReport &report, std::string kind, std::size_t k,
        const LRkDfaStats &stats, const ParseTable &table,
        std::size_t mergeConflicts = 0) {
    report.automata.push_back({
            std::move(kind), k, stats.states, stats.items, stats.transitions,
            table.conflicts().size(), mergeConflicts, std::nullopt});
    if (!table.hasConflicts()) {
        report.automata.back().storage = CompressedParseTable(table).statistics();
    }
}

const char *ambiguityStatusName(AmbiguitySearchStatus status) {
    switch (status) {
        case AmbiguitySearchStatus::WitnessFound: return "witness-found";
        case AmbiguitySearchStatus::Exhausted: return "exhausted";
        case AmbiguitySearchStatus::Inconclusive: return "inconclusive";
    }
    throw std::logic_error("unknown ambiguity status");
}

double seconds(Clock::duration duration) {
    return std::chrono::duration<double>(duration).count();
}

struct StageAggregate {
    std::size_t calls = 0;
    Clock::duration total{};
    Clock::duration maximum{};
    std::size_t maximumGrammar = 0;
};

std::map<std::string, StageAggregate> aggregateStageTimings(
        const CorpusLabelResult &result) {
    std::map<std::string, StageAggregate> aggregates;
    for (const CorpusGrammarReport &grammar: result.grammars) {
        for (const CorpusStageTiming &timing: grammar.timings) {
            StageAggregate &aggregate = aggregates[timing.stage];
            ++aggregate.calls;
            aggregate.total += timing.elapsed;
            if (aggregate.maximumGrammar == 0
                    || timing.elapsed > aggregate.maximum) {
                aggregate.maximum = timing.elapsed;
                aggregate.maximumGrammar = grammar.index;
            }
        }
    }
    return aggregates;
}

void classify(
        const Grammar &grammar, const CorpusLabelOptions &options,
        CorpusGrammarReport &report, const CorpusProgress &progress) {
    try {
        timedStage(report, progress, "ambiguity", [&] {
            const auto result = findAmbiguity(grammar, {
                    .maxLength = options.ambiguityMaxLength,
                    .treeLimit = options.ambiguityTreeLimit,
                    .order = std::nullopt,
                    .seed = options.ambiguitySeed,
            });
            report.ambiguityStatus = result.status();
            report.ambiguityDiagnostic = result.dump(grammar);
        });
    } catch (const GeneratorGrammarError &error) {
        report.generatorTrap = true;
        report.ambiguityDiagnostic = error.what();
    }

    if (report.ambiguityStatus == AmbiguitySearchStatus::WitnessFound) {
        if (!options.collectDetailedReport) {
            report.labels.emplace_back("[ambig]");
            return;
        }
        runAnalyses(grammar, report, progress);
        std::optional<FirstKAnalysis> first;
        timedStage(report, progress, "FIRST/FOLLOW(1)", [&] {
            first.emplace(grammar, 1);
            const FollowKAnalysis follow(grammar, *first);
            report.lookaheads.push_back(lookaheadStats(grammar, *first, follow));
        });
        report.labels.emplace_back("[ambig]");
        return;
    }

    runAnalyses(grammar, report, progress);

    std::optional<std::size_t> acceptedLookahead;
    std::optional<LRkDfa> acceptedDfa;
    for (std::size_t k = 1; k <= options.maxLookahead; ++k) {
        std::optional<FirstKAnalysis> first;
        if (options.collectDetailedReport) {
            timedStage(report, progress,
                    "FIRST/FOLLOW(" + std::to_string(k) + ")", [&] {
                first.emplace(grammar, k);
                const FollowKAnalysis follow(grammar, *first);
                report.lookaheads.push_back(lookaheadStats(grammar, *first, follow));
            });
        }
        timedStage(report, progress, "LR(" + std::to_string(k) + ")", [&] {
            std::optional<LRkDfa> dfa;
            if (options.stopAfterFirstConflict
                    && !options.collectDetailedReport) {
                const FirstKAnalysis probeFirst(grammar, k);
                auto probe = LRkDfa::buildUntilFirstConflict(grammar, probeFirst);
                if (probe.conflict) return;
                dfa.emplace(std::move(*probe.dfa));
            } else {
                dfa.emplace(first
                        ? LRkDfa(grammar, *first)
                        : LRkDfa(grammar, k));
            }

            const ParseTable table(*dfa);
            if (options.collectDetailedReport) {
                addAutomatonStats(
                        report, "LR", k, dfa->statistics(), table);
            }
            if (!table.hasConflicts()) {
                acceptedLookahead = k;
                acceptedDfa.emplace(std::move(*dfa));
            }
        });
        if (acceptedLookahead) break;
    }

    if (!acceptedLookahead) {
        report.labels.push_back("[notLR(" + std::to_string(options.maxLookahead) + ")]" );
        return;
    }

    report.labels.push_back("[LR(" + std::to_string(*acceptedLookahead) + ")]" );
    timedStage(report, progress,
            "LALR(" + std::to_string(*acceptedLookahead) + ")", [&] {
        const LALRkDfa lalr(*acceptedDfa);
        const ParseTable table{lalr};
        if (options.collectDetailedReport) {
            addAutomatonStats(
                    report, "LALR", *acceptedLookahead, lalr.statistics(),
                    table, table.mergeConflicts().size());
        }
        if (!table.hasConflicts()) {
            report.labels.push_back(
                    "[LALR(" + std::to_string(*acceptedLookahead) + ")]" );
        }
    });
    if (options.collectDetailedReport && *acceptedLookahead == 1) {
        timedStage(report, progress, "SLR(1)", [&] {
            LRkDfaStats stats{};
            const ParseTable table = ParseTable::slr(grammar, stats);
            addAutomatonStats(report, "SLR", 1, stats, table);
        });
    }
}

} // namespace

std::size_t CorpusLabelResult::unexpectedFailureCount() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(
            grammars, [](const CorpusGrammarReport &report) {
        return report.error.has_value();
    }));
}

CorpusLabelResult labelGrammarCorpus(
        const std::vector<std::string> &lines,
        const CorpusLabelOptions &options,
        const CorpusProgress &progress) {
    if (options.maxLookahead == 0) {
        throw std::invalid_argument("corpus classification requires maxLookahead >= 1");
    }

    const auto started = Clock::now();
    CorpusLabelResult result;
    std::vector<std::string> block;

    const auto finishBlock = [&] {
        if (block.empty()) return;
        if (!hasRule(block)) {
            result.lines.insert(result.lines.end(), block.begin(), block.end());
            block.clear();
            return;
        }

        CorpusGrammarReport report{};
        report.index = result.grammars.size() + 1;
        report.source = block;
        const auto grammarStarted = Clock::now();
        try {
            Grammar grammar = GrammarBuilder{}.build(block);
            classify(grammar, options, report, progress);
            installLabels(block, report.labels);
        } catch (const std::exception &error) {
            report.error = error.what();
        }
        report.elapsed = Clock::now() - grammarStarted;
        result.lines.insert(result.lines.end(), block.begin(), block.end());
        result.grammars.push_back(std::move(report));
        block.clear();
    };

    for (const std::string &line: lines) {
        if (isSeparator(line)) {
            finishBlock();
            result.lines.push_back(line);
        } else {
            block.push_back(line);
        }
    }
    finishBlock();
    result.elapsed = Clock::now() - started;
    return result;
}

std::string formatCorpusReport(
        const CorpusLabelResult &result,
        const CorpusLabelOptions &options,
        std::string_view buildConfiguration) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    out << "ZBIK CORPUS REPORT\n";
    out << "table_bytes=packed-estimate; baseline=sparse-exact; header=32; "
           "counts_ids=8; terminal=4; action=16; "
           "compression=default-reductions+shared-rows; excludes=grammar+allocator\n";
    out << "build=" << buildConfiguration << '\n';
    out << "grammars=" << result.grammars.size() << '\n';
    out << "total_seconds=" << seconds(result.elapsed) << '\n';
    out << "unexpected_failures=" << result.unexpectedFailureCount() << '\n';
    out << "max_lookahead=" << options.maxLookahead << '\n';
    out << "ambiguity_max_length=" << options.ambiguityMaxLength << '\n';
    out << "ambiguity_tree_limit=" << options.ambiguityTreeLimit << '\n';
    out << "ambiguity_seed=" << options.ambiguitySeed << "\n\n";

    out << "STAGE TOTALS\n";
    for (const auto &[stage, aggregate]: aggregateStageTimings(result)) {
        out << stage
            << " calls=" << aggregate.calls
            << " total_seconds=" << seconds(aggregate.total)
            << " max_seconds=" << seconds(aggregate.maximum)
            << " max_grammar=" << aggregate.maximumGrammar << '\n';
    }
    out << "END STAGE TOTALS\n\n";

    for (const CorpusGrammarReport &grammar: result.grammars) {
        out << "GRAMMAR " << grammar.index << '\n';
        out << "total_seconds=" << seconds(grammar.elapsed) << '\n';
        out << "labels=" << joinLabels(grammar.labels) << '\n';
        if (grammar.ambiguityStatus) {
            out << "ambiguity_status="
                << ambiguityStatusName(*grammar.ambiguityStatus) << '\n';
        } else if (grammar.generatorTrap) {
            out << "ambiguity_status=generator-trap\n";
        }
        if (grammar.ambiguityDiagnostic) {
            out << "ambiguity_diagnostic=" << *grammar.ambiguityDiagnostic << '\n';
        }
        if (grammar.error) out << "error=" << *grammar.error << '\n';

        out << "SOURCE\n";
        for (const std::string &line: grammar.source) out << line << '\n';
        out << "END SOURCE\n";

        out << "STAGES\n";
        for (const CorpusStageTiming &timing: grammar.timings) {
            out << timing.stage << " seconds=" << seconds(timing.elapsed) << '\n';
        }
        out << "END STAGES\n";

        out << "LOOKAHEAD SETS\n";
        for (const CorpusLookaheadStats &lookahead: grammar.lookaheads) {
            out << "k=" << lookahead.k
                << " first_words=" << lookahead.firstWords
                << " follow_words=" << lookahead.followWords
                << " max_first_words=" << lookahead.maxFirstWords
                << " max_follow_words=" << lookahead.maxFollowWords << '\n';
            for (const CorpusNonterminalSetStats &nonterminal: lookahead.nonterminals) {
                out << "  " << nonterminal.nonterminal
                    << " first=" << nonterminal.firstWords
                    << " follow=" << nonterminal.followWords << '\n';
            }
        }
        out << "END LOOKAHEAD SETS\n";

        out << "AUTOMATA\n";
        for (const CorpusAutomatonStats &automaton: grammar.automata) {
            out << automaton.kind << '(' << automaton.k << ')'
                << " states=" << automaton.states
                << " items=" << automaton.items
                << " transitions=" << automaton.transitions
                << " conflicts=" << automaton.conflicts;
            if (automaton.kind == "LALR") {
                out << " merge_conflicts=" << automaton.mergeConflicts;
            }
            if (automaton.storage) {
                const auto &s = *automaton.storage;
                out << " uncompressed_bytes=" << s.uncompressedBytes
                    << " compressed_bytes=" << s.compressedBytes
                    << " saved_percent=" << 100.0 * (1.0 -
                       static_cast<double>(s.compressedBytes) / s.uncompressedBytes)
                    << " unique_action_rows=" << s.uniqueActionRows
                    << " unique_goto_rows=" << s.uniqueGotoRows;
            }
            out << '\n';
        }
        out << "END AUTOMATA\n";
        out << "END GRAMMAR\n\n";
    }
    const CorpusAutomatonStats *largest = nullptr;
    std::size_t largestGrammar = 0;
    for (const auto &grammar: result.grammars) {
        for (const auto &automaton: grammar.automata) {
            if (automaton.storage && (!largest || automaton.storage->uncompressedBytes >
                    largest->storage->uncompressedBytes)) {
                largest = &automaton;
                largestGrammar = grammar.index;
            }
        }
    }
    if (largest) {
        const auto &s = *largest->storage;
        out << "LARGEST SUCCESSFUL TABLE grammar=" << largestGrammar
            << " kind=" << largest->kind << '(' << largest->k << ')'
            << " uncompressed_bytes=" << s.uncompressedBytes
            << " compressed_bytes=" << s.compressedBytes
            << " saved_percent=" << 100.0 * (1.0 -
               static_cast<double>(s.compressedBytes) / s.uncompressedBytes) << '\n';
    }
    return out.str();
}

} // namespace zbik
