#!/bin/python

import urllib.request, json
from pathlib import Path

mime_db_path = "https://raw.githubusercontent.com/jshttp/mime-db/master/db.json"

jsonurl = urllib.request.urlopen(mime_db_path).read()
data = json.loads(jsonurl.decode('utf-8'))

ext_dict={}

for attr, value in data.items():
    if "extensions" in value:
        for ext in value.get("extensions"):
            if ext in ext_dict:
                ext_dict[ext].append(attr)
            else:
                ext_dict[ext] = [attr]

f = open(Path(__file__).parent.absolute() / "mime-db.hpp", "w")
f.write("#pragma once\n\n")
f.write("#include <filesystem>\n")
f.write("#include <string>\n")
f.write("#include <unordered_map>\n\n")
f.write("namespace mime {\n")
f.write("\tinline const std::unordered_map<std::string, std::string> db = {\n")
for attr, value in ext_dict.items():
    f.write("\t\t{\"" + attr + "\", \"" + value[0] + "\"},\n")
f.write("\t};\n\n")

f.write("\t[[nodiscard]] inline std::string find(const std::filesystem::path& file) {\n")
f.write("\t\tauto ext = file.extension().generic_string();\n")
f.write("\t\tif (ext.empty())\n")
f.write("\t\t\treturn \"application/octet-stream\";\n")
f.write("\t\tstd::ranges::transform(ext, ext.begin(), [](const unsigned char c) { return std::tolower(c); });\n")
f.write("\t\tconst auto& ite = db.find(ext.substr(1));\n")
f.write("\t\treturn ite == db.end() ? \"application/octet-stream\" : ite->second;\n")
f.write("\t}\n")
f.write("}\n")

f.close()


