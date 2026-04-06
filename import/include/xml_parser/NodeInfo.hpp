#pragma once

#include <optional>
#include <vector>
#include <cstdint>
#include <string>
#include <variant>

enum class NodeKind {
    Boolean,
    Category,
    Command,
    Converter,
    Enumeration,
    Float,
    FloatConverter,
    FloatReg,
    FloatSwissKnife,
    Group,
    IntConverter,
    IntReg,
    IntSwissKnife,
    Integer,
    MaskedIntReg,
    Port,
    Register,
    StringReg,
    StructEntry,
    StructReg,
    SwissKnife,

    Unknown
};

inline constexpr std::pair<const char*, NodeKind> kKnownTags[] = {
    {"Boolean", NodeKind::Boolean},
    {"Category", NodeKind::Category},
    {"Command", NodeKind::Command},
    {"Converter", NodeKind::Converter},
    {"Enumeration", NodeKind::Enumeration},
    {"Float", NodeKind::Float},
    {"FloatConverter", NodeKind::FloatConverter},
    {"FloatReg", NodeKind::FloatReg},
    {"FloatSwissKnife", NodeKind::FloatSwissKnife},
    {"Group", NodeKind::Group},
    {"IntConverter", NodeKind::IntConverter},
    {"IntReg", NodeKind::IntReg},
    {"IntSwissKnife", NodeKind::IntSwissKnife},
    {"Integer", NodeKind::Integer},
    {"MaskedIntReg", NodeKind::MaskedIntReg},
    {"Port", NodeKind::Port},
    {"Register", NodeKind::Register},
    {"StringReg", NodeKind::StringReg},
    {"StructReg", NodeKind::StructReg},
    {"SwissKnife", NodeKind::SwissKnife},
};

inline NodeKind tagToKind(const std::string& tag) {
    for (const auto& [t, kind] : kKnownTags) {
        if (tag == t) return kind;
    }
    return NodeKind::Unknown;
}

inline const char* kindToTag(NodeKind kind) {
    if (kind != NodeKind::Unknown) {
        for (const auto& [tag, k] : kKnownTags)
            if (kind == k) return tag;
    }
    return nullptr;
}

enum class Representation {
    Boolean,
    HexNumber,
    IEEE754,
    Linear,
    Logarithmic,
    PureNumber,
};

enum class Endianess {
    Big,
    Little,
};

enum class Slope {
    Automatic,
    Decreasing,
    Increasing,
    Varying,
};

enum class Sign {
    Signed,
    Unsigned,
};

enum class AccessMode {
    RO,
    RW,
    WO,
};

enum class CachingMode {
    NoCache,
    WriteAround,
    WriteThrough,
};

struct BitField {
    uint8_t lsb = 0;
    uint8_t msb = 0;
    uint16_t parentStruct = 0; // index in nodeTable
};

struct MemoryAccessor {
    virtual ~MemoryAccessor() = default;
    virtual uint64_t read(uint64_t addr, uint8_t len, Endianess endian) const = 0;
    virtual void     write(uint64_t addr, uint8_t len, uint64_t val, Endianess endian) = 0;
};

enum class NodeAccessPath {
    Value,
    Min,
    Max,
    Inc
};

struct NodeReference {
    uint16_t nodeId;
    NodeAccessPath path = NodeAccessPath::Value;
};

using VariableSubstitution = std::variant<
    NodeReference,                    // reference
    std::pair<uint32_t, uint32_t>     // subexpression range in expressions vector
>;

struct VariableBinding {
    std::string name;       // variable name from Name attribute of pVariable
    uint16_t nodeId;        // referenced node id
};

struct EnumEntry {
    std::string name;
    uint64_t value;
    std::optional<uint16_t> pIsImplemented;
};

struct NodeInfo {
    uint16_t id = 0;
    NodeKind kind = NodeKind::Unknown;

    // NodeElementTemplate
    std::optional<uint16_t> pIsImplemented;
    std::optional<uint16_t> pIsAvailable;
    std::optional<uint16_t> pIsLocked;
    std::optional<AccessMode> imposedAccess;
    std::optional<uint16_t> pAlias;

    // RegisterElementTemplate
    std::optional<uint64_t> address;
    std::optional<uint16_t> pAddress;
    std::optional<uint16_t> pIndex;
    std::optional<uint16_t> Offset;
    std::optional<uint64_t> length;
    std::optional<uint16_t> pLength;
    std::optional<AccessMode> accessMode;
    std::optional<uint16_t> pPort;
    std::optional<CachingMode> cachable;
    std::optional<uint64_t> pollingTime;
    std::vector<uint16_t> invalidators;

    // Value/Limits
    std::optional<uint64_t> min;
    std::optional<uint16_t> pMin;
    std::optional<uint64_t> max;
    std::optional<uint16_t> pMax;
    std::optional<Representation> representation;

    // IntegerType
    std::optional<uint64_t> inc;
    std::optional<uint16_t> pInc;

    // CommandType
    std::optional<uint64_t> value;
    std::optional<uint16_t> pValue;
    std::optional<uint64_t> commandValue;
    std::optional<uint16_t> pCommandValue;

    // BooleanType
    std::optional<uint64_t> onValue;
    std::optional<uint64_t> offValue;

    // FloatType
    std::optional<std::string> unit;

    // ConverterType
    std::vector<VariableBinding> variables;
    std::optional<std::pair<size_t, size_t>> formulaFrom;
    std::optional<std::pair<size_t, size_t>> formulaTo;
    std::optional<Slope> slope;

    // SwissKnifeType
    std::optional<std::pair<size_t, size_t>> formula;

    // PortType
    std::optional<uint64_t> chunkID;
    std::optional<uint16_t> pchunkID;
    std::optional<bool> swapEndianess;

    // IntRegType
    std::optional<Sign> sign;
    std::optional<Endianess> endianess;

    // MaskedIntRegType
    std::optional<BitField> bitField;

    // EnumEntryType
    std::vector<EnumEntry> enumEntries;

    // Constant value (from <Value> element)
    std::optional<uint64_t> constantValue;

    // for NodeMap building & debug/log only
    std::string name;
    std::string structName;
};
