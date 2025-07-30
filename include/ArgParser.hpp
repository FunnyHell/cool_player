#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <set>
#include <memory>

class ArgParser {
    public:
        struct Arg {
            std::string description;
            bool requiresValue = false;
            std::function<void(const std::string&)> callback;
            std::set<std::string> aliases;
        };

    void addOption(const std::vector<std::string>& names,
                   const std::string& description,
                   bool requiresValue = false,
                   std::function<void(const std::string&)> callback = {});

        void parse(int argc, char* argv[]);

        void printHelp() const;

        const std::vector<std::string>& getPositionalArgs() const;

    private:
        std::unordered_map<std::string, Arg*> argMap_;
        std::vector<std::unique_ptr<Arg>> args_;
        std::vector<std::string> positionalArgs_;

};
