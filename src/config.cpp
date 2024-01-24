#include <string>
#include <iostream>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <stdexcept>
#include "config.h"

using namespace std;

Config::Config(const filesystem::path file_path) : file_path(file_path) {
    if (!file_path.empty())
        read(file_path);
}

void Config::read(const filesystem::path file_path){
    fstream file;
    string line, name, value;
    int line_no = 0;
    smatch match;
    file.open(file_path.c_str(), ios::out | ios::in);
    option.clear();
    while (getline(file, line)){
        line_no++;
        line = regex_replace(line, comment, "");
        if (line.size()){
            if (regex_match(line, match, definition)){
                name = match[1].str();
                value = match[2].str();
                transform(name.begin(), name.end(), name.begin(), ::tolower);
                if (option.count(name))
                    cout << "[" << file_path << "]:" << line_no << " " << name << " is already defined.\n";
                else
                   option[name] = value;
            }else{
                cout << "[" << file_path << "]:" << line_no << " Wrong definition.\n";
                throw;
            }
        }
    }
    file.close();
}
string Config::get(string name, string default_value, regex re_cases){
    if (option.count(name)){
        string value = option[name];
        if (regex_match(value, re_cases))
            return value;
        cout << "[" << file_path << "]: '" << name << "' has wrong value.\n";
    }else
        cout << "[" << file_path << "]: '" << name << "' was not defined.\n";
    return default_value;
}

double Config::get(string name, double default_value, double minimal, double maximal){
    double value = stod(get(name, to_string(default_value), regex(R"(\d*\.?\d+)")));
    if (value < minimal || value > maximal){
        cout << "[" << file_path << "] " << name << " has out of " << minimal << "..." << maximal << " range.\n";
        return min(max(minimal, value), maximal);
    }
    return value;
}

int Config::get_case_index(string name, int default_value, regex re_cases){
    smatch match;
    if (option.count(name)){
        if (regex_match(option[name], match, re_cases))
            for (int i = 1; i < (int)match.size(); i++)
                if (match[i].matched)
                    return i - 1;
        cout << "[" << file_path << "]: '" << name << "' has wrong value.\n";
    }else
        cout << "[" << file_path << "]: '" << name << "' was not defined.\n";
    return default_value;
}
