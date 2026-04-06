/*
 * load_xml.cpp - Parse a GenICam XML device description
 *
 * Loads a GenICam XML file (e.g. downloaded by dump_device_xml) and
 * builds an in-memory node map with all features, registers, and formulas.
 *
 * Build: Part of xml_parser CMake project
 * Usage: ./load_xml <device.xml>
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "NodeMap.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <device.xml>\n";
        return 1;
    }

    // Read the XML file into a string
    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open " << argv[1] << "\n";
        return 1;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string xml_data = ss.str();
    file.close();

    std::cout << "Loaded " << xml_data.size() << " bytes from " << argv[1] << "\n";

    // Parse the XML and build the node map
    NodeMap& nm = NodeMap::instance();
    if (!nm.init(xml_data.c_str())) {
        std::cerr << "Error: Failed to parse GenICam XML\n";
        return 2;
    }

    // Print summary of public features
    std::cout << "\n=== Public Features ===\n";
    for (const auto& [name, id] : nm.features()) {
        const NodeInfo* node = nm.findNode(id);
        if (!node) continue;

        const char* tag = kindToTag(node->kind);
        std::cout << "  " << name << " [" << (tag ? tag : "?") << "]";

        if (node->address)
            std::cout << " addr=0x" << std::hex << *node->address << std::dec;
        if (node->length)
            std::cout << " len=" << *node->length;
        if (node->constantValue)
            std::cout << " val=" << *node->constantValue;
        if (node->accessMode) {
            switch (*node->accessMode) {
                case AccessMode::RO: std::cout << " RO"; break;
                case AccessMode::RW: std::cout << " RW"; break;
                case AccessMode::WO: std::cout << " WO"; break;
            }
        }
        if (!node->enumEntries.empty()) {
            std::cout << " enum={";
            for (size_t i = 0; i < node->enumEntries.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << node->enumEntries[i].name << "=" << node->enumEntries[i].value;
            }
            std::cout << "}";
        }
        std::cout << "\n";
    }

    // Print node count by type
    std::cout << "\n=== Node Summary ===\n";
    std::unordered_map<NodeKind, int> counts;
    for (const auto& node : nm.nodes())
        counts[node.kind]++;
    for (const auto& [kind, count] : counts) {
        const char* tag = kindToTag(kind);
        std::cout << "  " << (tag ? tag : "Unknown") << ": " << count << "\n";
    }

    return 0;
}
