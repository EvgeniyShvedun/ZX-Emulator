class Config {
    public:
        Config(const std::filesystem::path file_path = "");
        void read(const std::filesystem::path file_path);
        bool exist(std::string name) { return bool(option.count(name)); };
        std::string get(std::string name, std::string default_value = std::string(), std::regex re_cases = std::regex(R"(.*)"));
        double get(std::string name, double default_value, double minimal, double maximal);
        int get_case(std::string name, int default_value, std::regex re_cases);
    private:
        const std::filesystem::path file_path;
        const std::regex comment = std::regex(R"(\s*;.*)");
        const std::regex definition = std::regex(R"((\S+)\s*=\s*(\S+))");
        std::unordered_map<std::string, std::string> option;
};
