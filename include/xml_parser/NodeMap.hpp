#pragma once

#include <iostream>
#include <unordered_map>
#include <functional>
#include <limits>
#include <vector>
#include <string>
#include <string_view>
#include "pugixml.hpp"

#include "NodeInfo.hpp"
#include "MathCalc.hpp"

class NodeMap {
    std::unordered_map<std::string, uint16_t> publicFeatures_;
    std::unordered_map<std::string, uint16_t> nameToId_;
    std::vector<MathEvaluate::Token> expressions_;
    std::vector<NodeInfo> nodeTable_;
    bool error_ = false;

    std::optional<uint16_t> nextNodeId() {
        if (nodeTable_.size() > static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
            std::cerr << "Error: Node table exceeded uint16_t capacity (" 
                      << std::numeric_limits<uint16_t>::max() << ")\n";
            error_ = true;
            return std::nullopt;
        }
        return static_cast<uint16_t>(nodeTable_.size());
    }

    std::pair<uint32_t, uint32_t> parseExpression(const std::string& expression) {
        MathEvaluate::Parser<double> parser;

        auto tokens = parser.parse(expression, [&](std::string_view name) -> std::optional<MathEvaluate::Token> {
            std::string sname(name);
            auto it = nameToId_.find(sname);
            if (it != nameToId_.end())
                return MathEvaluate::Variable{static_cast<std::size_t>(it->second)};
            return std::nullopt;
        });

        if (tokens.empty())
            return {0, 0};

        auto from = static_cast<uint32_t>(expressions_.size());
        expressions_.insert(expressions_.end(), tokens.begin(), tokens.end());
        auto to = static_cast<uint32_t>(expressions_.size());
        return {from, to};
    }

    // Find xml source tag using Node's name or structName
    std::optional<pugi::xml_node> findXMLTag(pugi::xml_node root, const NodeInfo& node) {
        const char* tag = kindToTag(node.kind);
        if (!tag) return std::nullopt;

        pugi::xml_node xml;
        if (node.name.empty() && !node.structName.empty())
            xml = root.find_child_by_attribute(tag, "Comment", node.structName.c_str());
        else if (!node.name.empty())
            xml = root.find_child_by_attribute(tag, "Name", node.name.c_str());

        if (!xml) return std::nullopt;
        return xml;
    }

    // Stage 0: Recursively walk Root.Category and build node table
    void recursivelyBuildNode(pugi::xml_node root, const std::string& name, uint16_t& autoStructId) {
        if (error_) return;
        if (nameToId_.contains(name)) return;

        for (const auto& [tag, kind] : kKnownTags) {
            if (auto xml = root.find_child_by_attribute(tag, "Name", name.c_str())) {
                auto idOpt = nextNodeId();
                if (!idOpt) return;
                uint16_t id = *idOpt;
                nameToId_[name] = id;

                NodeInfo node;
                node.id = id;
                node.name = name;
                node.kind = kind;
                nodeTable_.push_back(std::move(node));

                auto resolveRef = [&](const char* childTag) {
                    if (auto c = xml.child(childTag))
                        recursivelyBuildNode(root, c.child_value(), autoStructId);
                };

                resolveRef("pValue");
                resolveRef("pMin");
                resolveRef("pMax");
                resolveRef("pConverter");
                resolveRef("pAddress");
                resolveRef("pPort");
                resolveRef("pIsImplemented");
                resolveRef("pIsLocked");

                std::string_view tagView = tag;
                if (tagView == "SwissKnife" || tagView == "IntSwissKnife" ||
                    tagView == "FloatSwissKnife" || tagView == "Converter" ||
                    tagView == "IntConverter" || tagView == "FloatConverter") {
                    for (auto v : xml.children("pVariable"))
                        recursivelyBuildNode(root, v.child_value(), autoStructId);
                }
                return;
            }
        }

        // Check StructReg entries
        for (auto sreg : root.children("StructReg")) {
            for (auto entry : sreg.children("StructEntry")) {
                std::string entryName = entry.attribute("Name").value();
                if (entryName == name) {
                    std::string structName = sreg.attribute("Name").as_string();
                    if (structName.empty()) {
                        if (auto comment = sreg.attribute("Comment"))
                            structName = comment.as_string();
                        else
                            structName = "StructReg_auto_" + std::to_string(autoStructId++);
                    }

                    if (!nameToId_.contains(structName)) {
                        auto sidOpt = nextNodeId();
                        if (!sidOpt) return;
                        uint16_t sid = *sidOpt;
                        NodeInfo snode;
                        snode.id = sid;
                        snode.kind = NodeKind::StructReg;
                        snode.structName = structName;
                        nodeTable_.push_back(std::move(snode));
                        nameToId_[structName] = sid;
                    }

                    uint16_t structId = nameToId_[structName];
                    auto idOpt = nextNodeId();
                    if (!idOpt) return;
                    uint16_t id = *idOpt;
                    nameToId_[name] = id;

                    BitField bf;
                    if (auto bit = entry.child("Bit")) {
                        int b = std::stoi(bit.child_value());
                        bf.lsb = bf.msb = static_cast<uint8_t>(b);
                    } else {
                        auto lsb = entry.child("LSB");
                        auto msb = entry.child("MSB");
                        if (lsb && msb) {
                            bf.lsb = static_cast<uint8_t>(std::stoi(lsb.child_value(), nullptr, 0));
                            bf.msb = static_cast<uint8_t>(std::stoi(msb.child_value(), nullptr, 0));
                        } else {
                            std::cerr << "Warning: Missing LSB/MSB/Bit for StructEntry: " << name << "\n";
                            return;
                        }
                    }
                    bf.parentStruct = structId;

                    NodeInfo snode;
                    snode.id = id;
                    snode.name = entryName;
                    snode.kind = NodeKind::StructEntry;
                    snode.bitField = bf;
                    nodeTable_.push_back(std::move(snode));
                    return;
                }
            }
        }

        std::cerr << "Warning: No XML definition found for: " << name << "\n";
    }

    void collectRootFeatures(pugi::xml_node root, uint16_t& autoStructId) {
        auto rootCat = root.find_child_by_attribute("Category", "Name", "Root");
        if (!rootCat) return;

        std::function<void(pugi::xml_node)> walkCategory = [&](pugi::xml_node cat) {
            for (auto pf : cat.children("pFeature")) {
                std::string name = pf.child_value();
                if (error_) return;

                if (auto subcat = root.find_child_by_attribute("Category", "Name", name.c_str()))
                    walkCategory(subcat);
                else {
                    recursivelyBuildNode(root, name, autoStructId);
                    if (nameToId_.contains(name))
                        publicFeatures_[name] = nameToId_[name];
                }
            }
        };
        walkCategory(rootCat);
    }

    // Stage 1: Resolve references (pValue, pMin, etc.)
    void resolveReferences(pugi::xml_node root) {
        auto setRef = [&](pugi::xml_node xml, const char* tag, std::optional<uint16_t>& field) {
            if (auto c = xml.child(tag)) {
                auto it = nameToId_.find(c.child_value());
                if (it != nameToId_.end())
                    field = it->second;
            }
        };

        for (auto& node : nodeTable_) {
            if (node.kind == NodeKind::StructEntry) continue;
            if (node.kind == NodeKind::StructReg && node.name.empty() &&
                node.structName.starts_with("StructReg_auto_"))
                continue;

            auto xmlOpt = findXMLTag(root, node);
            if (!xmlOpt) continue;
            auto xml = *xmlOpt;

            setRef(xml, "pValue", node.pValue);
            setRef(xml, "pMin", node.pMin);
            setRef(xml, "pMax", node.pMax);
            setRef(xml, "pAddress", node.pAddress);
            setRef(xml, "pPort", node.pPort);
            setRef(xml, "pIsImplemented", node.pIsImplemented);
            setRef(xml, "pIsLocked", node.pIsLocked);
        }
    }

    // Stage 2: Resolve attributes
    void resolveAttributes(pugi::xml_node root) {
        for (auto& node : nodeTable_) {
            if (node.kind == NodeKind::StructEntry) continue;

            auto xmlOpt = findXMLTag(root, node);
            if (!xmlOpt) continue;
            auto xml = *xmlOpt;

            if (auto a = xml.child("ImposedAccessMode")) {
                std::string m = a.child_value();
                if (m == "RO") node.imposedAccess = AccessMode::RO;
                else if (m == "RW") node.imposedAccess = AccessMode::RW;
                else if (m == "WO") node.imposedAccess = AccessMode::WO;
            }
            if (auto a = xml.child("AccessMode")) {
                std::string m = a.child_value();
                if (m == "RO") node.accessMode = AccessMode::RO;
                else if (m == "RW") node.accessMode = AccessMode::RW;
                else if (m == "WO") node.accessMode = AccessMode::WO;
            }
            if (auto v = xml.child("Length"))
                try { node.length = std::stoull(v.child_value(), nullptr, 0); } catch (...) {}
            if (auto v = xml.child("Address"))
                try { node.address = std::stoull(v.child_value(), nullptr, 0); } catch (...) {}
            if (auto v = xml.child("Endianess")) {
                std::string e = v.child_value();
                if (e == "LittleEndian") node.endianess = Endianess::Little;
                else if (e == "BigEndian") node.endianess = Endianess::Big;
            }
            if (auto v = xml.child("Representation")) {
                std::string r = v.child_value();
                if (r == "Linear") node.representation = Representation::Linear;
                else if (r == "Logarithmic") node.representation = Representation::Logarithmic;
                else if (r == "Boolean") node.representation = Representation::Boolean;
                else if (r == "HexNumber") node.representation = Representation::HexNumber;
                else if (r == "PureNumber") node.representation = Representation::PureNumber;
                else if (r == "IEEE754") node.representation = Representation::IEEE754;
            }
            if (auto v = xml.child("Value"))
                try { node.constantValue = std::stoull(v.child_value(), nullptr, 0); } catch (...) {}
            if (auto v = xml.child("Min"))
                try { node.min = std::stoull(v.child_value(), nullptr, 0); } catch (...) {}
            if (auto v = xml.child("Max"))
                try { node.max = std::stoull(v.child_value(), nullptr, 0); } catch (...) {}
            if (auto v = xml.child("Inc"))
                try { node.inc = std::stoull(v.child_value(), nullptr, 0); } catch (...) {}
            if (auto v = xml.child("Sign")) {
                std::string s = v.child_value();
                if (s == "Signed") node.sign = Sign::Signed;
                else if (s == "Unsigned") node.sign = Sign::Unsigned;
            }
            if (auto v = xml.child("Unit"))
                node.unit = v.child_value();
            if (auto v = xml.child("CommandValue"))
                try { node.commandValue = std::stoull(v.child_value(), nullptr, 0); } catch (...) {}
            if (auto v = xml.child("OnValue"))
                try { node.onValue = std::stoull(v.child_value(), nullptr, 0); } catch (...) {}
            if (auto v = xml.child("OffValue"))
                try { node.offValue = std::stoull(v.child_value(), nullptr, 0); } catch (...) {}
            if (auto v = xml.child("PollingTime"))
                try { node.pollingTime = std::stoull(v.child_value(), nullptr, 0); } catch (...) {}
            if (auto v = xml.child("Cachable")) {
                std::string c = v.child_value();
                if (c == "NoCache") node.cachable = CachingMode::NoCache;
                else if (c == "WriteAround") node.cachable = CachingMode::WriteAround;
                else if (c == "WriteThrough") node.cachable = CachingMode::WriteThrough;
            }
            if (auto v = xml.child("Slope")) {
                std::string s = v.child_value();
                if (s == "Automatic") node.slope = Slope::Automatic;
                else if (s == "Increasing") node.slope = Slope::Increasing;
                else if (s == "Decreasing") node.slope = Slope::Decreasing;
                else if (s == "Varying") node.slope = Slope::Varying;
            }

            // BitField
            if (auto bit = xml.child("Bit")) {
                uint8_t b = static_cast<uint8_t>(std::stoi(bit.child_value()));
                node.bitField = BitField{.lsb = b, .msb = b, .parentStruct = node.id};
            } else {
                auto lsb = xml.child("LSB");
                auto msb = xml.child("MSB");
                if (lsb && msb) {
                    node.bitField = BitField{
                        .lsb = static_cast<uint8_t>(std::stoi(lsb.child_value(), nullptr, 0)),
                        .msb = static_cast<uint8_t>(std::stoi(msb.child_value(), nullptr, 0)),
                        .parentStruct = node.id
                    };
                }
            }

            // Enumeration
            if (node.kind == NodeKind::Enumeration) {
                for (auto entry : xml.children("EnumEntry")) {
                    auto valNode = entry.child("Value");
                    if (!valNode) continue;
                    try {
                        EnumEntry ee;
                        ee.name = entry.attribute("Name").as_string();
                        ee.value = std::stoull(valNode.child_value(), nullptr, 0);
                        if (auto pImpl = entry.child("pIsImplemented")) {
                            auto it = nameToId_.find(pImpl.child_value());
                            if (it != nameToId_.end()) ee.pIsImplemented = it->second;
                        }
                        node.enumEntries.push_back(std::move(ee));
                    } catch (...) {}
                }
            }
        }
    }

    // Stage 3: Resolve formulas
    void resolveFormulas(pugi::xml_node root) {
        for (auto& node : nodeTable_) {
            auto xmlOpt = findXMLTag(root, node);
            if (!xmlOpt) continue;
            auto xml = *xmlOpt;

            if (auto f = xml.child("FormulaTo")) {
                auto [from, to] = parseExpression(f.child_value());
                if (to > from) node.formulaTo = {from, to};
            }
            if (auto f = xml.child("FormulaFrom")) {
                auto [from, to] = parseExpression(f.child_value());
                if (to > from) node.formulaFrom = {from, to};
            }
            if (auto f = xml.child("Formula")) {
                auto [from, to] = parseExpression(f.child_value());
                if (to > from) node.formula = {from, to};
            }

            // pVariable bindings
            switch (node.kind) {
                case NodeKind::SwissKnife:
                case NodeKind::FloatSwissKnife:
                case NodeKind::IntSwissKnife:
                case NodeKind::Converter:
                case NodeKind::FloatConverter:
                case NodeKind::IntConverter:
                    for (auto pv : xml.children("pVariable")) {
                        std::string varName = pv.attribute("Name").as_string();
                        std::string refName = pv.child_value();
                        auto it = nameToId_.find(refName);
                        if (it != nameToId_.end())
                            node.variables.push_back(VariableBinding{varName, it->second});
                        else
                            std::cerr << "Warning: pVariable unknown: " << refName
                                      << " in " << node.name << "\n";
                    }
                    break;
                default:
                    break;
            }
        }
    }

public:
    bool init(const char* xml_data) {
        nodeTable_.clear();
        nameToId_.clear();
        publicFeatures_.clear();
        expressions_.clear();
        error_ = false;

        pugi::xml_document doc;
        if (!doc.load_string(xml_data)) {
            std::cerr << "Error: Failed to parse XML\n";
            return false;
        }

        auto root = doc.child("RegisterDescription");
        if (!root) {
            std::cerr << "Error: Missing RegisterDescription root element\n";
            return false;
        }

        uint16_t autoStructId = 0;
        collectRootFeatures(root, autoStructId);
        if (error_) return false;

        resolveReferences(root);
        resolveAttributes(root);
        resolveFormulas(root);

        std::cout << "Indexed " << nodeTable_.size() << " nodes, "
                  << publicFeatures_.size() << " public features\n";
        return true;
    }

    const std::vector<NodeInfo>& nodes() const { return nodeTable_; }
    const std::unordered_map<std::string, uint16_t>& features() const { return publicFeatures_; }

    const NodeInfo* findNode(const std::string& name) const {
        auto it = nameToId_.find(name);
        if (it == nameToId_.end()) return nullptr;
        return &nodeTable_[it->second];
    }

    const NodeInfo* findNode(uint16_t id) const {
        if (id >= nodeTable_.size()) return nullptr;
        return &nodeTable_[id];
    }

    std::optional<double> evaluateExpression(
        size_t from, size_t to,
        std::function<std::optional<double>(const MathEvaluate::Variable&)> varResolver = {}) const
    {
        if (from >= to || to > expressions_.size()) return std::nullopt;
        MathEvaluate::Calculator<double> calc(varResolver);
        std::span<const MathEvaluate::Token> tokens(expressions_.data() + from, to - from);
        return calc.eval(tokens);
    }

    static NodeMap& instance() {
        static NodeMap nm;
        return nm;
    }
};
