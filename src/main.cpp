#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <cstddef>
#include <string>

#include "SP.h"
#include "Lexer.h"
#include "Parser.h"
#include "CodeGen.h"
#include "DiagnosticUI.h"
#include "CorrectionEngine.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./stobx <filename.stobx>\n";
        return 1;
    }

    const std::string filename = argv[1];
    const char* skipFile = ".stobx_skip_correction";
    bool skipCorrectionPrompt = false;

    {
        std::ifstream skipFlag(skipFile);
        if (skipFlag.good()) {
            skipCorrectionPrompt = true;
            skipFlag.close();
            std::remove(skipFile);
        }
    }

    SP processor;
    if (!processor.process(filename)) {
        std::cerr << "Error: Preprocessor failed on file " << filename << "\n";
        return 1;
    }

    std::string sourceCode;
    {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filename << "\n";
            return 1;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        sourceCode = buffer.str();
    } // file is safely closed here before any potential overwrite

    try {
        Lexer lexer(sourceCode);
        auto tokens = lexer.tokenize();

        // Catch invalid/unrecognized tokens early before parsing
        bool hasLexError = false;
        for (const auto& token : tokens) {
            if (token.type == TokenType::UNKNOWN) {
                std::cerr << "Lexer Error: Line " << token.line
                          << ", Column " << token.column
                          << ": Unexpected token '" << token.value << "'\n";
                hasLexError = true;
            }
        }
        if (hasLexError) {
            return 1;
        }

        Parser parser(tokens);
        auto ast = parser.parse();

        if (!parser.getDiagnostics().empty() && !skipCorrectionPrompt) {
            DiagnosticUI ui;
            CorrectionEngine correctionEngine;

            std::string currentSourceCode = sourceCode;
            std::ptrdiff_t offsetAdjustment = 0;
            bool fileModified = false;

            for (const auto& diagnostic : parser.getDiagnostics()) {
                ui.showDiagnostic(diagnostic);

                std::string correction =
                    correctionEngine.suggestCorrection(diagnostic.id);

                ui.showCorrection(correction);

                Diagnostic adjustedDiagnostic = diagnostic;
                adjustedDiagnostic.offset = static_cast<std::size_t>(
                    static_cast<std::ptrdiff_t>(diagnostic.offset) + offsetAdjustment
                );

                std::string preview =
                    correctionEngine.createPreview(
                        currentSourceCode,
                        adjustedDiagnostic
                    );

                if (preview.empty()) {
                    std::cout << "Could not create correction preview.\n";
                    continue;
                }

                if (ui.askForCorrection()) {
                    std::ofstream outputFile(
                        filename,
                        std::ios::out | std::ios::trunc
                    );

                    if (!outputFile.is_open()) {
                        std::cerr << "Error: Could not save corrected file.\n";
                        return 1;
                    }

                    outputFile << preview;
                    outputFile.close();

                    offsetAdjustment +=
                        static_cast<std::ptrdiff_t>(preview.size()) -
                        static_cast<std::ptrdiff_t>(currentSourceCode.size());

                    currentSourceCode = preview;
                    fileModified = true;
                } else {
                    std::cout << "\nCorrection cancelled.\n";

                    std::ofstream outSkipFlag(skipFile);
                    if (!outSkipFlag.is_open()) {
                        std::cerr << "Error: Could not save temporary state.\n";
                        return 1;
                    }
                    outSkipFlag.close();

                    return 1;
                }
            }

            if (fileModified) {
                std::cout << "\nFile updated with corrections. Please re-run ./stobx "
                          << filename << "\n";
            }
            return 1;
        }

        if (!parser.getErrors().empty()) {
            for (const auto& error : parser.getErrors()) {
                std::cerr << error << "\n";
            }
            return 1;
        }

        CodeGen codegen;
        codegen.generate(ast.get());
        codegen.printIR();

        // -lm ensures external math functions (extern) link cleanly
        int compileResult = std::system(
            "clang output.ll -lm -o stobx_program"
        );

        if (compileResult != 0) {
            std::cerr << "Error: Failed to compile STOBX program\n";
            return 1;
        }

        int runResult = std::system(
            "./stobx_program"
        );

        if (runResult != 0) {
            std::cerr << "Error: Program exited with non-zero status ("
                      << runResult << ")\n";
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
