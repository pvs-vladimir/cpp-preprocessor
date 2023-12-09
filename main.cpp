#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

bool ProcessFile(const path& in_file, ofstream& out, const vector<path>& include_directories);

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

void PrintProcessError(const string& file, const string& in_file, int line) {
    cout << "unknown include file "s << file << " at file "s << in_file 
        << " at line "s << line << endl;
}

bool ProcessDirectories(const path& match_path, ofstream& out, const vector<path>& include_directories) {
    for (const path& dir : include_directories) {
        path p_dir = dir / match_path;
        if (ProcessFile(p_dir, out, include_directories)) {
            return true;
        }
    }
    return false;
}

bool ProcessFile(const path& in_file, ofstream& out, const vector<path>& include_directories) {
    static regex regex1(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex regex2(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    int line = 1;
    
    ifstream in(in_file);
    if (!in) {
        return false;
    }
    
    while(in) {
        string str;
        getline(in, str);
        if (in.eof() && str.empty()) {
            return true;
        }
        
        smatch m;
        if (regex_match(str, m, regex1)) {
            path match_path = string(m[1]);
            path p = in_file.parent_path() / match_path;
            if (!ProcessFile(p, out, include_directories)) {
                if (!ProcessDirectories(match_path, out, include_directories)) {
                    PrintProcessError(string(m[1]), in_file.string(), line);
                    return false;
                }
            }
        } else if (regex_match(str, m, regex2)) {
            path match_path = string(m[1]);
            if (!ProcessDirectories(match_path, out, include_directories)) {
                PrintProcessError(string(m[1]), in_file.string(), line);
                return false;
            }
        } else {
            out << str << endl;
        }
        ++line;
    }
    
    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in(in_file);
    if (!in) {
        return false;
    }
    ofstream out(out_file);
    
    return ProcessFile(in_file, out, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}