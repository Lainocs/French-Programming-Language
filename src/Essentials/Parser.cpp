#include "Parser.h"

namespace FPL::Parser {

    void Parser::FichierInstruction(Data::Data &data, std::optional<FPL::Paquet::Paquet> paquet) {
        auto possibleArg = ExpectIdentifiant(data);
        if (possibleArg.has_value() && possibleArg->TokenText == "lire") {
            auto possibleVariable = ExpectIdentifiant(data);
            if (possibleVariable.has_value()) {
                auto possibleFileName = ExpectValue(data);

                if (possibleFileName.has_value() && possibleFileName->StatementType.Type == Types::STRING) {
                    std::ifstream file { possibleFileName->StatementName};
                    if (!file) { FICHIER_fileunknow(data); }
                    std::string fileContent((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));

                    if (data.isVariable(possibleVariable->TokenText)) {
                        std::optional<VariableDef> var = data.getVariable(possibleVariable->TokenText);
                        if (var.has_value()) {
                            data.updateValue(var->VariableName, fileContent);
                        } else {
                            FICHIER_varunknow(data);
                        }
                    } else {
                        VariableDef newVar;
                        if (paquet.has_value()) {
                            newVar.VariableName = paquet->PaquetName + "." + possibleVariable->TokenText;
                        } else {
                            newVar.VariableName = possibleVariable->TokenText;
                        }
                        newVar.VariableValue = fileContent;
                        newVar.VariableType = Types::Types("texte", Types::STRING);
                        newVar.NeedDelete = false;
                        newVar.IsGlobal = false;

                        data.addVariableToMap(newVar.VariableName,
                                              newVar.VariableValue,
                                              newVar.VariableType,
                                              newVar.NeedDelete,
                                              newVar.IsGlobal);
                    }

                    if (!ExpectOperator(data, ";")) {
                        forgotEndInstructionOperator(data);
                    }
                } else {
                    FICHIER_stringtypetogetfile(data);
                }
            } else {
                FICHIER_needvariable(data);
            }
        } else if (possibleArg.has_value() && possibleArg->TokenText == "ecrire") {
            auto possibleFileName = ExpectValue(data);
            if (possibleFileName.has_value()) {
                if (ExpectEgalOperators(data)) {
                    auto possibleNewContent = ExpectValue(data);
                    if (possibleNewContent.has_value()) {
                        std::ofstream file(possibleFileName->StatementName);
                        if (!file) { FICHIER_fileunknow(data); }
                        file << possibleNewContent->StatementName;
                        if (!ExpectOperator(data, ";")) {
                            forgotEndInstructionOperator(data);
                        }
                    } else {
                        forgotValue(data);
                    }
                } else {
                    forgotEgalOperators(data);
                }
            } else{
                FICHIER_fileunknow(data);
            }
        } else {
            FICHIER_wrongargument(data);
        }
    }

    void Parser::PaquetInstruction(FPL::Data::Data &data, const std::optional<FPL::FonctionDef>& fonction, std::optional<FPL::Paquet::Paquet> paquet) {
        auto nomPaquet = ExpectIdentifiant(data);
        if (nomPaquet.has_value()) {
            FPL::Paquet::Paquet Paquet;

            if (paquet.has_value()) {
                Paquet.PaquetName = paquet->PaquetName + "." + nomPaquet->TokenText;
            } else{
                Paquet.PaquetName = nomPaquet->TokenText;
            }

            if (ExpectOperator(data, "{").has_value()) {
                int totalInstructionInDefinition = 0;

                while (data.current_token != data.end_token) {
                    auto currentToken = data.current_token;
                    if (currentToken->TokenText == "definir" || currentToken->TokenText == "paquet") {
                        totalInstructionInDefinition += 1;
                    }

                    if (currentToken->TokenType == FPL::Tokenizer::CHAINE_LITTERAL) {
                        currentToken->TokenText = "\"" + currentToken->TokenText += "\"";
                    }

                    if (currentToken->TokenText == "}") {
                        if (totalInstructionInDefinition > 0) {
                            totalInstructionInDefinition -= 1;
                        } else {
                            break;
                        }
                    }

                    Paquet.PaquetContent.push_back(currentToken->TokenText);
                    data.incrementeTokens(data);
                }

                if (!ExpectOperator(data, "}").has_value()) {
                    PAQUET_close(data);
                }

                std::vector<Tokenizer::Token> FileCode_Tokens = FPL::Tokenizer::TokenBuilder::ParseToken(FPL::Instruction::FunctionUtils::ReturnStringVector(Paquet.PaquetContent));

                auto data_paquet = executeContentCode(FileCode_Tokens, fonction, Paquet, data);

                for (auto const& variables : data_paquet.Map_Variables) {
                    auto it = std::find(data_paquet.Map_Variables.begin(), data_paquet.Map_Variables.end(), variables);
                    if (it != data_paquet.Map_Variables.end()) {
                        data.addVariableToMap(it->second.VariableName,
                                              it->second.VariableValue,
                                              it->second.VariableType,
                                              it->second.NeedDelete,
                                              it->second.IsGlobal);
                    }
                }

                for (auto const& variables : data_paquet.Map_Fonctions) {
                    auto it = std::find(data_paquet.Map_Fonctions.begin(), data_paquet.Map_Fonctions.end(), variables);
                    if (it != data_paquet.Map_Fonctions.end()) {
                        data.addFunctionToMap(it->second.FonctionName,
                                              it->second.FonctionType,
                                              it->second.AllFonctionArguments,
                                              it->second.FonctionContentCode,
                                              it->second.FonctionNumberArgument,
                                              it->second.ReturnValue);
                    }
                }
            } else {
                PAQUET_open(data);
            }
        } else {
            PAQUET_forgotname(data);
        }
    }

    void Parser::ImporterInstruction(FPL::Data::Data& data, const std::optional<FPL::FonctionDef>& fonction) {
        auto possibleFileName = ExpectValue(data);
        if (possibleFileName.has_value() && possibleFileName->StatementType.Type == Types::STRING) {

            std::ifstream file { possibleFileName->StatementName};
            if (!file) {
                IMPORT_needfilename(data);
            }

            if (!ExpectOperator(data, ";").has_value()) {
                forgotEndInstructionOperator(data);
            }

            std::string Import_ContentCode((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));
            std::vector<Tokenizer::Token> ImportFile_Tokens = FPL::Tokenizer::TokenBuilder::ParseToken(Import_ContentCode);
            auto data_ImportFile = executeContentCode(ImportFile_Tokens, fonction, std::nullopt, data);

            for (auto const& variables : data_ImportFile.Map_Variables) {
                auto it = std::find(data_ImportFile.Map_Variables.begin(), data_ImportFile.Map_Variables.end(), variables);
                if (it != data_ImportFile.Map_Variables.end()) {
                    if (it->second.IsGlobal) {
                        data.addVariableToMap(it->second.VariableName,
                                              it->second.VariableValue,
                                              it->second.VariableType,
                                              it->second.NeedDelete,
                                              it->second.IsGlobal);
                    }
                }
            }

            for (auto const& variables : data_ImportFile.Map_Fonctions) {
                auto it = std::find(data_ImportFile.Map_Fonctions.begin(), data_ImportFile.Map_Fonctions.end(), variables);
                if (it != data_ImportFile.Map_Fonctions.end()) {
                    data.addFunctionToMap(it->second.FonctionName,
                                          it->second.FonctionType,
                                          it->second.AllFonctionArguments,
                                          it->second.FonctionContentCode,
                                          it->second.FonctionNumberArgument,
                                          it->second.ReturnValue);
                }
            }
        } else {
            IMPORT_needfilename(data);
        }
    }

    void Parser::RenvoyerInstruction(FPL::Data::Data &data, std::optional<FPL::FonctionDef> fonction) {
        if (!fonction.has_value()) {
            RETURN_noinfonction(data);
        }

        std::string finalValue;

        auto returnValue = ExpectValue(data);
        if (returnValue.has_value()) {
            finalValue = returnValue->StatementName;
            data.ReturnType = returnValue->StatementType;
        } else {
            auto returnId = ExpectIdentifiant(data);

            if (returnId.has_value() && data.isVariable(returnId->TokenText) && returnId->TokenText != "vrai" && returnId->TokenText != "faux") {
                auto variable = data.getVariable(returnId->TokenText);
                if (variable.has_value()) { finalValue = variable->VariableValue; data.ReturnType = variable->VariableType; }
            } else if (returnId.has_value() && returnId->TokenText == "vrai") {
                finalValue = "vrai";
                data.ReturnType = Types::Types("bool", Types::BOOL);
            } else if (returnId.has_value() && returnId->TokenText == "faux") {
                finalValue = "faux";
                data.ReturnType = Types::Types("bool", Types::BOOL);
            } else if (returnId.has_value() && fonction->isArgument(returnId->TokenText)) {
                auto argument = fonction->getArgument(returnId->TokenText);
                if (argument.has_value()) { finalValue = argument->ArgumentValue; data.ReturnType = argument->ArgumentType; }
            } else {
                forgotValue(data);
            }
        }

        if (!ExpectOperator(data, ";").has_value()) {
            forgotEndInstructionOperator(data);
        }

        fonction->ReturnValue = finalValue;
        data.HasReturnValue = true;
        data.ReturnValue = finalValue;
    }

    void Parser::AppelerInstruction(FPL::Data::Data& data) {
        auto possibleFunctionName = ExpectIdentifiant(data);
        if (possibleFunctionName.has_value()) {
            auto fonction = data.getFonction(possibleFunctionName->TokenText);

            std::vector<Tokenizer::Token> FileCode_Tokens = FPL::Tokenizer::TokenBuilder::ParseToken(FPL::Instruction::FunctionUtils::ReturnStringVector(fonction->FonctionContentCode));

            if (fonction->FonctionNumberArgument < 1) {
                if (!ExpectOperator(data, ";").has_value()) {
                    forgotEndInstructionOperator(data);
                }
            } else if (ExpectOperator(data, ":").has_value() && fonction->FonctionNumberArgument > 0) {
                int totalArgs = fonction->FonctionNumberArgument;

                if (totalArgs == 0) { FONCTION_noneedargs(data); }

                while (totalArgs > 0) {
                    auto possibleArgName = ExpectIdentifiant(data);
                    if (!possibleArgName.has_value()) {
                        FONCTION_forgotargumenttogivevalue(data);
                    }

                    auto possibleArgValue = ExpectValue(data);
                    std::string possibleArgFinalValue;
                    if (possibleArgValue.has_value()) {
                        possibleArgFinalValue = possibleArgValue->StatementName;
                    } else {
                        auto possibleBoolValue = ExpectIdentifiant(data);
                        if (possibleBoolValue.has_value()) {
                            if (possibleBoolValue->TokenText == "vrai" || possibleBoolValue->TokenText == "faux") {
                                possibleArgFinalValue = possibleBoolValue->TokenText;
                            } else {
                                forgotValue(data);
                            }
                        } else {
                            forgotValue(data);
                        }
                    }

                    if (!fonction->isArgument(possibleArgName->TokenText)) {
                        FONCTION_argumentDoesNotExist(data);
                    }

                    auto target_argument = fonction->getArgument(possibleArgName->TokenText);
                    if (!target_argument.has_value()) {
                        FONCTION_didnotfindarg(data);
                    }
                    fonction->updateValueOfArgument(target_argument.value(), possibleArgFinalValue);

                    totalArgs -= 1;
                    if (totalArgs == 0) { break; }
                    else if (totalArgs > 0) {
                        if (!ExpectOperator(data, ",").has_value()) {
                            FONCTION_forgotaddarg(data);
                        }
                    }
                }

                if (!ExpectOperator(data, ";").has_value()) {
                    forgotEndInstructionOperator(data);
                }
            } else {
                forgotEndInstructionOperator(data);
            }

            auto data_f = executeContentCode(FileCode_Tokens, fonction, std::nullopt, data);

            for (auto const& variables : data_f.Map_Variables) {
                auto it = std::find(data_f.Map_Variables.begin(), data_f.Map_Variables.end(), variables);
                if (it != data_f.Map_Variables.end()) {
                    if (it->second.IsGlobal) {
                        data.addVariableToMap(it->second.VariableName,
                                              it->second.VariableValue,
                                              it->second.VariableType,
                                              it->second.NeedDelete,
                                              it->second.IsGlobal);
                    }
                }
            }
        } else {
            FONCTION_forgotnametocall(data);
        }
    }

    void Parser::DefinirInstruction(FPL::Data::Data& data, std::optional<FPL::Paquet::Paquet> paquet) {
        auto FonctionPossibleName = ExpectIdentifiant(data);
        if (FonctionPossibleName.has_value()) {
            FonctionDef fonction;

            if (paquet.has_value()) {
                fonction.FonctionName = paquet->PaquetName + "." + FonctionPossibleName->TokenText;
            } else {
                fonction.FonctionName = FonctionPossibleName->TokenText;
            }

            if (ExpectOperator(data, "(").has_value()) {
                // On récupère le(s) argument(s) de la fonction (ou pas)
                while (!ExpectOperator(data, ")").has_value()) {
                    auto possibleArgType = ExpectType(data);
                    if (!possibleArgType.has_value()) {
                        FONCTION_forgotargtype(data);
                    }

                    auto possibleArgName = ExpectIdentifiant(data);
                    if (!possibleArgName.has_value()) {
                        FONCTION_forgotargname(data);
                    }

                    FonctionArgumentDef argument;
                    argument.ArgumentName = possibleArgName->TokenText;
                    argument.ArgumentType = Types::Types(possibleArgType->Name, possibleArgType->Type);

                    if (fonction.isArgument(possibleArgName->TokenText)) {
                        FONCTION_argumentexist(data);
                    }

                    fonction.FonctionNumberArgument += 1;
                    fonction.AllFonctionArguments[possibleArgName->TokenText] = argument;

                    if (ExpectOperator(data, ")").has_value()) {
                        break;
                    } else if (!ExpectOperator(data, ",").has_value()) {
                        FONCTION_forgotaddarg(data);
                    }
                }

                if (ExpectOperator(data, "{").has_value()) {
                    // On récupère le contenu de la fonction

                    int totalInstructionInDefinition = 0;

                    while (true) {
                        auto currentToken = data.current_token;
                        if (currentToken->TokenText == "definir" || currentToken->TokenText == "paquet") {
                            totalInstructionInDefinition += 1;
                        }

                        if (currentToken->TokenType == FPL::Tokenizer::CHAINE_LITTERAL) {
                            currentToken->TokenText = "\"" + currentToken->TokenText += "\"";
                        }

                        fonction.FonctionContentCode.push_back(currentToken->TokenText);
                        data.incrementeTokens(data);

                        if (ExpectOperator(data, "}").has_value()) {
                            if (totalInstructionInDefinition > 0) {
                                fonction.FonctionContentCode.emplace_back("}");
                                totalInstructionInDefinition -= 1;
                            } else {
                                break;
                            }
                        } else if (data.current_token == data.end_token) {
                            fonction.FonctionContentCode.emplace_back("}");
                            break;
                        }
                    }

                    data.Map_Fonctions[fonction.FonctionName] = fonction;
                } else {
                    FONCTION_forgotinsertcode(data);
                }
            } else {
                FONCTION_forgotfirstParenthese(data);
            }
        } else {
            forgotName(data);
        }
    }

    void Parser::ConversionInstruction(FPL::Data::Data& data) {
        auto possibleVariable = ExpectIdentifiant(data);
        if (possibleVariable.has_value() && data.isVariable(possibleVariable->TokenText)) {
            auto variable = data.getVariable(possibleVariable->TokenText);
            auto possibleNewType = ExpectType(data);
            if (possibleNewType.has_value()) {
                if (ExpectOperator(data, ";").has_value()) {
                    if (possibleNewType->Type == variable->VariableType.Type) {
                        CONVERSION_sameTypeVariable(data);
                    }
                    data.updateType(variable->VariableName, possibleNewType->Name, possibleNewType->Type);
                    variable = data.getVariable(variable->VariableName);
                    if (variable->VariableType.Type == Types::BUILTIN_TYPE::INT) {
                        if (variable->VariableValue == "vrai") {
                            data.updateValue(variable->VariableName, "1");
                        } else if (variable->VariableValue == "faux") {
                            data.updateValue(variable->VariableName, "0");
                        } else {
                            int v = stringToInt(variable->VariableValue, "");
                            data.updateValue(variable->VariableName, std::to_string(v));
                        }
                    } else if (variable->VariableType.Type == Types::BUILTIN_TYPE::DOUBLE) {
                        double v = stringToDouble(variable->VariableValue, "");
                        data.updateValue(variable->VariableName, std::to_string(v));
                    } else if (variable->VariableType.Type == Types::BUILTIN_TYPE::BOOL) {
                        if (variable->VariableValue == "1") {
                            data.updateValue(variable->VariableName, "vrai");
                        } else if (variable->VariableValue == "0") {
                            data.updateValue(variable->VariableName, "faux");
                        } else {
                            CONVERSION_boolNumberFromInt(data);
                        }
                    }
                } else {
                    forgotEndInstructionOperator(data);
                }
            } else {
                forgotType(data);
            }
        } else {
            variableDoesNotExist(data);
        }
    }

    void Parser::MathInstruction(FPL::Data::Data& data, std::optional<FPL::Paquet::Paquet> paquet) {
        auto possibleName = ExpectIdentifiant(data);
        if (possibleName.has_value()) {
            if (ExpectEgalOperators(data)) {
                std::vector<std::string> MathInsContent_Vector;
                data.decrementeTokens(data);
                while (!ExpectOperator(data, ";").has_value()) {
                    auto CurrT = data.incrementeAndGetToken(data);
                    if (CurrT->TokenText != ";") {
                        MathInsContent_Vector.push_back(CurrT->TokenText);
                    }

                    if (ExpectOperator(data, ";").has_value()) {
                        break;
                    }
                }
                std::string MathInsContent = StringVectorTOString(MathInsContent_Vector);

                std::vector<MathParser::Token> contentMathTokens = MathParser::TokenBuilding::ParserTokens(MathInsContent);
                double result = MATHPARSER_Parser(contentMathTokens);

                if (data.isVariable(possibleName->TokenText)) {
                    auto variable = data.getVariable(possibleName->TokenText);
                    std::string newTypeName = "decimal";
                    data.updateValue(variable->VariableName, std::to_string(result));
                    data.updateType(variable->VariableName, newTypeName, FPL::Types::BUILTIN_TYPE::DOUBLE);
                } else {
                    VariableDef variable;

                    if (paquet.has_value()) {
                        variable.VariableName = paquet->PaquetName + "." + possibleName->TokenText;
                    } else {
                        variable.VariableName = possibleName->TokenText;
                    }

                    variable.VariableType = FPL::Types::Types("decimal", FPL::Types::DOUBLE);
                    variable.VariableValue = std::to_string(result);
                    data.addVariableToMap(variable.VariableName,
                                          variable.VariableValue,
                                          variable.VariableType,
                                          variable.NeedDelete,
                                          variable.IsGlobal
                    );
                }
            } else {
                forgotEgalOperators(data);
            }
        } else {
            forgotName(data);
        }
    }

    void Parser::InputInstruction(FPL::Data::Data& data, std::optional<FPL::Paquet::Paquet> paquet) {
        auto possibleType = ExpectType(data);
        if (possibleType.has_value()) {
            auto possibleName = ExpectIdentifiant(data);
            if (possibleName.has_value()) {
                if (ExpectEgalOperators(data)) { // Si on affiche une valeur classique/d'une variable dans la console...
                    auto Content_CIN_Value = ExpectValue(data);
                    auto Content_CIN_Id = ExpectIdentifiant(data);

                    if (ExpectOperator(data, ";").has_value()) {
                        if (Content_CIN_Id.has_value() && data.isVariable(Content_CIN_Id->TokenText)) {
                            auto variable = data.getVariable(Content_CIN_Id->TokenText);
                            std::cout << variable->VariableValue << std::endl;
                        } else if (Content_CIN_Value.has_value()) {
                            std::cout << Content_CIN_Value->StatementName << std::endl;
                        }

                        std::string finalValue = FPL::Instruction::Inputs::returnInputFinalValue(possibleType->Type, data);

                        if (possibleType->Type == FPL::Types::BOOL) {
                            if (finalValue != "vrai" && finalValue != "faux") {
                                BoolNotLikeValue(data);
                            }
                        }

                        if (data.isVariable(possibleName->TokenText)) {
                            auto variable = data.getVariable(Content_CIN_Id->TokenText);
                            variable->VariableValue = finalValue;
                        } else {
                            VariableDef variable;
                            if (paquet.has_value()) {
                                variable.VariableName = paquet->PaquetName + "." + possibleName->TokenText;
                            } else {
                                variable.VariableName = possibleName->TokenText;
                            }
                            variable.VariableValue = finalValue;
                            variable.VariableType = FPL::Types::Types(possibleType->Name, possibleType->Type);

                            data.addVariableToMap(variable.VariableName,
                                                  variable.VariableValue,
                                                  variable.VariableType,
                                                  variable.NeedDelete,
                                                  variable.IsGlobal
                            );
                        }
                    } else {
                        forgotEgalOperators(data);
                    }
                } else { // Si on n'affiche rien dans la console...
                    if (ExpectOperator(data, ";").has_value()) {
                        std::string finalValue = FPL::Instruction::Inputs::returnInputFinalValue(possibleType->Type, data);

                        if (possibleType->Type == FPL::Types::BOOL) {
                            if (finalValue != "vrai" && finalValue != "faux") {
                                BoolNotLikeValue(data);
                            }
                        }

                        VariableDef variable;
                        if (paquet.has_value()) {
                            variable.VariableName = paquet->PaquetName + "." + possibleName->TokenText;
                        } else {
                            variable.VariableName = possibleName->TokenText;
                        }
                        variable.VariableValue = finalValue;
                        variable.VariableType = FPL::Types::Types(possibleType->Name, possibleType->Type);

                        data.addVariableToMap(variable.VariableName,
                                              variable.VariableValue,
                                              variable.VariableType,
                                              variable.NeedDelete,
                                              variable.IsGlobal
                        );
                    } else {
                        forgotEgalOperators(data);
                    }
                }
            } else {
                forgotName(data);
            }
        } else {
            forgotType(data);
        }
    }

    void Parser::ChangerInstruction(FPL::Data::Data& data) {
        auto Nom_Variable = ExpectIdentifiant(data);
        if (Nom_Variable.has_value() && data.isVariable(Nom_Variable->TokenText)) {

            auto variable = data.getVariable(Nom_Variable->TokenText);

            if (ExpectEgalOperators(data)) {
                auto New_Value = ExpectValue(data);

                if (ExpectOperator(data, ";").has_value()) {
                    if (New_Value.has_value()) {
                        if (variable->VariableType.Type == New_Value->StatementType.Type) {
                            data.updateValue(variable->VariableName, New_Value->StatementName);
                        } else {
                            differentTypes(data);
                        }
                    } else {
                        if (variable->VariableType.Type == Types::BOOL) {
                            auto bool_value = ExpectIdentifiant(data);
                            if (bool_value->TokenText != "vrai" && bool_value->TokenText != "faux") {
                                wrongTypeForBool(data);
                            }

                            data.updateValue(variable->VariableName, bool_value->TokenText);
                        } else {
                            forgotValue(data);
                        }
                    }
                } else {
                    forgotEndInstructionOperator(data);
                }
            } else{
                forgotEgalOperators(data);
            }
        } else {
            variableDoesNotExist(data);
        }
    }

    void Parser::VariableInstruction(FPL::Data::Data &data, std::optional<FPL::FonctionDef>& fonction, const std::optional<FPL::Paquet::Paquet>& paquet) {
        auto possibleType = ExpectType(data);
        if (possibleType.has_value()) {
            VariableDef variable;
            std::string possibleParamVar = "N/A";
            auto possibleVariableName = ExpectIdentifiant(data);

            if (possibleVariableName.has_value()) {
                if (possibleVariableName->TokenText == "globale") {
                    variable.IsGlobal = true;
                    variable.NeedDelete = false;
                    possibleParamVar = "globale";
                } else if (possibleVariableName->TokenText == "fonction") {
                    possibleParamVar = "fonction";
                }

                if (possibleParamVar != "N/A") {
                    possibleVariableName = ExpectIdentifiant(data);
                }

                if (paquet.has_value()) {
                    variable.VariableName = paquet->PaquetName + "." + possibleVariableName->TokenText;
                } else {
                    variable.VariableName = possibleVariableName->TokenText;
                }

                if (ExpectEgalOperators(data)) {
                    if (fonction.has_value() && !variable.IsGlobal) { variable.NeedDelete = true; }
                    // Si la variable est déclaré dans une fonction, à la fin de l'execution elle sera delete.

                    auto possibleValue = ExpectValue(data); // Valeur classique genre 5, 5.2, "salut"
                    if (possibleValue.has_value() && possibleParamVar != "fonction") {
                        if (possibleType->Type == Types::BOOL) {
                            BoolNotLikeValue(data);
                        } else {
                            if (possibleValue->StatementType.Name != possibleType->Name) { // On vérifie si les deux types sont les mêmes
                                differentTypes(data);
                            }

                            if (ExpectOperator(data, ";").has_value()) {
                                if (data.isVariable(possibleVariableName->TokenText)) {
                                    variableAlreadyExist(data);
                                }
                                variable.VariableValue = possibleValue->StatementName;
                                variable.VariableType = FPL::Types::Types(possibleType->Name, possibleType->Type);

                                data.addVariableToMap(variable.VariableName,
                                                      variable.VariableValue,
                                                      variable.VariableType,
                                                      variable.NeedDelete,
                                                      variable.IsGlobal
                                );
                            } else {
                                forgotEndInstructionOperator(data);
                            }
                        }
                    }
                    else { // Pour les variables déjà déclaré ou le type bool + pour les returns
                        auto possibleId = ExpectIdentifiant(data);
                        if (possibleId.has_value()) {
                            if (possibleParamVar == "fonction") {
                                variable.VariableType = Types::Types(possibleType->Name, possibleType->Type);

                                auto possibleFunction = data.getFonction(possibleId->TokenText);
                                if (possibleFunction.has_value()) {

                                    std::vector<Tokenizer::Token> FileCode_Tokens = FPL::Tokenizer::TokenBuilder::ParseToken(FPL::Instruction::FunctionUtils::ReturnStringVector(possibleFunction->FonctionContentCode));

                                    if (ExpectOperator(data, ":").has_value() && possibleFunction->FonctionNumberArgument > 0) {
                                        int totalArgs = possibleFunction->FonctionNumberArgument;

                                        if (totalArgs == 0) { FONCTION_noneedargs(data); }

                                        while (totalArgs > 0) {
                                            auto possibleArgName = ExpectIdentifiant(data);
                                            if (!possibleArgName.has_value()) {
                                                FONCTION_forgotargumenttogivevalue(data);
                                            }

                                            auto possibleArgValue = ExpectValue(data);
                                            std::string possibleArgFinalValue;
                                            if (possibleArgValue.has_value()) {
                                                possibleArgFinalValue = possibleArgValue->StatementName;
                                            } else {
                                                auto possibleBoolValue = ExpectIdentifiant(data);
                                                if (possibleBoolValue.has_value()) {
                                                    if (possibleBoolValue->TokenText == "vrai" || possibleBoolValue->TokenText == "faux") {
                                                        possibleArgFinalValue = possibleBoolValue->TokenText;
                                                    } else {
                                                        forgotValue(data);
                                                    }
                                                } else {
                                                    forgotValue(data);
                                                }
                                            }

                                            if (!possibleFunction->isArgument(possibleArgName->TokenText)) {
                                                FONCTION_argumentDoesNotExist(data);
                                            }

                                            auto target_argument = possibleFunction->getArgument(
                                                    possibleArgName->TokenText);

                                            if (!target_argument.has_value()) {
                                                FONCTION_didnotfindarg(data);
                                            }

                                            possibleFunction->updateValueOfArgument(target_argument.value(),possibleArgFinalValue);

                                            totalArgs -= 1;
                                            if (totalArgs == 0) { break; }
                                            else if (totalArgs > 0) {
                                                if (!ExpectOperator(data, ",").has_value()) {
                                                    FONCTION_forgotaddarg(data);
                                                }
                                            }
                                        }
                                    }

                                    if (!ExpectOperator(data, ";").has_value()) {
                                        forgotEndInstructionOperator(data);
                                    }

                                    auto data_f = executeContentCode(FileCode_Tokens, possibleFunction, std::nullopt, data);

                                    if (!data_f.HasReturnValue) {
                                        RETURN_noreturn(data);
                                    }

                                    std::string returnValue = data_f.ReturnValue;
                                    Types::Types returnType = data_f.ReturnType;

                                    if (returnType.Type != variable.VariableType.Type) {
                                        RETURN_wrongtype(data);
                                    }

                                    for (auto const& variables : data_f.Map_Variables) {
                                        auto it = std::find(data_f.Map_Variables.begin(), data_f.Map_Variables.end(), variables);
                                        if (it != data_f.Map_Variables.end()) {
                                            if (it->second.IsGlobal) {
                                                data.addVariableToMap(it->second.VariableName,
                                                                      it->second.VariableValue,
                                                                      it->second.VariableType,
                                                                      it->second.NeedDelete,
                                                                      it->second.IsGlobal);
                                            }
                                        }
                                    }

                                    variable.VariableValue = returnValue;

                                    data.addVariableToMap(variable.VariableName,
                                                          variable.VariableValue,
                                                          variable.VariableType,
                                                          variable.NeedDelete,
                                                          variable.IsGlobal);
                                } else {
                                    FONCTION_doesnotexist(data);
                                }
                            } else {
                                if (data.isVariable(possibleId->TokenText)) {
                                    if (ExpectOperator(data, ";").has_value()) {
                                        auto oldVar  = data.getVariable(possibleId->TokenText);
                                        if (oldVar.has_value()) {
                                            variable.VariableValue = oldVar->VariableValue;
                                            variable.VariableType = FPL::Types::Types(possibleType->Name, possibleType->Type);

                                            if (variable != oldVar) {
                                                differentTypes(data);
                                            }

                                            data.addVariableToMap(variable.VariableName,
                                                                  variable.VariableValue,
                                                                  variable.VariableType,
                                                                  variable.NeedDelete,
                                                                  variable.IsGlobal
                                            );
                                        }
                                    } else {
                                        forgotEndInstructionOperator(data);
                                    }
                                } else if (possibleId->TokenText == "vrai" || possibleId->TokenText == "faux") {
                                    if (possibleType->Type == Types::BOOL) {
                                        if (ExpectOperator(data, ";").has_value()) {
                                            variable.VariableValue = possibleId->TokenText;
                                            variable.VariableType = FPL::Types::Types("bool", Types::BOOL);

                                            data.addVariableToMap(variable.VariableName,
                                                                  variable.VariableValue,
                                                                  variable.VariableType,
                                                                  variable.NeedDelete,
                                                                  variable.IsGlobal
                                            );
                                        } else {
                                            forgotEndInstructionOperator(data);
                                        }
                                    } else {
                                        wrongTypeForBool(data);
                                    }
                                }
                            }
                        } else {
                            forgotValue(data);
                        }
                    }
                } else{
                    forgotEgalOperators(data);
                }
            } else {
                forgotName(data);
            }
        } else {
            forgotType(data);
        }
    }

    void Parser::PrintInstruction(FPL::Data::Data& data, std::optional<FPL::FonctionDef>& fonction) {
        while (!ExpectOperator(data, ";").has_value()) {
            auto Value = ExpectValue(data);
            if (Value.has_value()) {
                std::cout << Value->StatementName;
            }

            auto Id = ExpectIdentifiant(data);
            if (Id.has_value()) {
                if (data.isVariable(Id->TokenText)) {
                    auto var = data.getVariable(Id->TokenText);
                    FPL::Instruction::Prints::managementPrint_VARIABLE(data, var.value());
                } else if (fonction.has_value() && fonction->isArgument(Id->TokenText)) {
                    auto arg = fonction->getArgument(Id->TokenText);
                    FPL::Instruction::Prints::managementPrint_ARGUMENT(data, arg);
                } else {
                    variableDoesNotExist(data);
                }
            }
            if (ExpectOperator(data, ";").has_value()) {
                break;
            }
        }
        std::cout << std::endl;
    }



    bool Parser::ManagerInstruction(FPL::Data::Data &data, std::optional<FPL::FonctionDef> fonction, const std::optional<FPL::Paquet::Paquet>& paquet) {
        auto Instruction = ExpectIdentifiant(data);
        if (Instruction.has_value()) {
            if (Instruction->TokenText == "envoyer") {
                PrintInstruction(data, fonction);
                return true;
            } else if (Instruction->TokenText == "variable") {
                VariableInstruction(data, fonction, paquet);
                return true;
            } else if (Instruction->TokenText == "changer") {
                ChangerInstruction(data);
                return true;
            } else if (Instruction->TokenText == "saisir") {
                InputInstruction(data, paquet);
                return true;
            } else if (Instruction->TokenText == "math") {
                MathInstruction(data, paquet);
                return true;
            } else if (Instruction->TokenText == "convertir") {
                ConversionInstruction(data);
                return true;
            } else if (Instruction->TokenText == "definir") {
                DefinirInstruction(data, paquet);
                return true;
            } else if (Instruction->TokenText == "appeler") {
                AppelerInstruction(data);
                return true;
            } else if (Instruction->TokenText == "renvoyer") {
                RenvoyerInstruction(data, fonction);
                return true;
            } else if (Instruction->TokenText == "importer") {
                ImporterInstruction(data, fonction);
                return true;
            } else if (Instruction->TokenText == "paquet") {
                PaquetInstruction(data, fonction, paquet);
                return true;
            } else if (Instruction->TokenText == "fichier") {
                FichierInstruction(data, paquet);
                return true;
            }
        }
        return false;
    }

    Data::Data Parser::executeContentCode(std::vector<FPL::Tokenizer::Token>& Tokens, const std::optional<FPL::FonctionDef>& fonction, const std::optional<FPL::Paquet::Paquet>& paquet, FPL::Data::Data& universalData) {
        Data::Data data(Tokens);

        for (auto const& v : universalData.Map_Variables) {
            auto it = std::find(universalData.Map_Variables.begin(), universalData.Map_Variables.end(), v);
            if (it != universalData.Map_Variables.end()) {
                data.addVariableToMap(it->second.VariableName,
                                      it->second.VariableValue,
                                      it->second.VariableType,
                                      it->second.NeedDelete,
                                      it->second.IsGlobal);
            }
        }

        while (data.current_token != data.end_token) {
            if (ManagerInstruction(data, fonction, paquet)) {

            } else {
                if (data.current_token->TokenText.empty()
                    || data.current_token->TokenType == FPL::Tokenizer::ESPACE_VIDE
                    || data.current_token->TokenText == " ") {
                    continue;
                }

                std::cerr << "Identifier inconnu : " << data.current_token->TokenText << " : "
                          << data.current_token->TokenLineNumber << std::endl;
                ++data.current_token;
            }
        }
        return data;
    }

    void Parser::ParserCode(std::vector<FPL::Tokenizer::Token>& Tokens) {
        Data::Data data(Tokens);

        while (data.current_token != data.end_token) {
            if (ManagerInstruction(data, std::nullopt, std::nullopt)) {

            } else {
                if (data.current_token->TokenText.empty()
                    || data.current_token->TokenType == FPL::Tokenizer::ESPACE_VIDE
                    || data.current_token->TokenText == " ") {
                    continue;
                }

                std::cerr << "Identifier inconnu : " << data.current_token->TokenText << " : "
                          << data.current_token->TokenLineNumber << std::endl;
                ++data.current_token;
            }
        }
    }
}