#include "ArgParser.hpp"
#include <iostream>

void ArgParser::addOption(const std::vector<std::string>& names,
                          const std::string& description,
                          bool requiresValue,
                          std::function<void(const std::string&)> callback) {
    auto arg = std::make_unique<Arg>();
    arg->description = description;
    arg->requiresValue = requiresValue;
    arg->callback = callback;
    arg->aliases.insert(names.begin(), names.end());

    for (const auto& name : names) {
        argMap_[name] = arg.get();
    }

    args_.push_back(std::move(arg));
}

void ArgParser::parse(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        std::string token = argv[i];
        std::string key, value;

        if (token.rfind("--", 0) == 0) {
            auto pos = token.find("=");
            if (pos != std::string::npos) {
                key = token.substr(2, pos - 2);
                value = token.substr(pos + 1);
            } else {
                key = token.substr(2);
                auto it = argMap_.find(key);
                if (it != argMap_.end() &&
                    it->second->requiresValue &&
                    i + 1 < argc) {
                        value = argv[++i];
                }
            }
        }
        else if (token.rfind("-", 0) == 0 && token.length() >= 2) {
            key = token.substr(1);
            auto it = argMap_.find(key);
            if (it != argMap_.end() &&
                it->second->requiresValue &&
                i + 1 < argc) {
                    value = argv[++i];
            }
        }
        else {
            positionalArgs_.push_back(token);
            continue;
        }

        if (argMap_.count(key)) {
            auto arg = argMap_[key];
            if (arg->callback) {
                arg->callback(value);
            }
        } else if (!key.empty()) {
            std::cerr << "Uknown argument: " << token << "\n";
        }
    }
}
