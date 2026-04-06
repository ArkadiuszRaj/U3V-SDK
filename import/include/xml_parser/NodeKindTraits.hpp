#pragma once
#include <cstdint>

#include "NodeInfo.hpp"

// Compile-time traits for each NodeKind.
// Describes which element groups are present on each node type.
//
// | NodeKind         | Node | Reg | Int | Float | Cmd | Enum | Conv | Swiss | Port | Str | Bit | Val | Lim |
// |------------------|------|-----|-----|-------|-----|------|------|-------|------|-----|-----|-----|-----|

template<NodeKind K>
struct NodeKindTraits {
    static constexpr bool hasNodeElement = false;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = false;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::Boolean> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::Category> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = false;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::Command> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = true;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = false;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::Converter> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = true;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::Enumeration> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = true;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::Float> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = true;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = true;
};

template<> struct NodeKindTraits<NodeKind::FloatConverter> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = true;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = true;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::FloatReg> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = true;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = true;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = false;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::FloatSwissKnife> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = true;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = true;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::Group> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = false;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::IntConverter> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = true;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = true;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::IntReg> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = true;
    static constexpr bool hasIntegerElement = true;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = true;
};

template<> struct NodeKindTraits<NodeKind::IntSwissKnife> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = true;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = true;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::Integer> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = true;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = true;
};

template<> struct NodeKindTraits<NodeKind::MaskedIntReg> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = true;
    static constexpr bool hasIntegerElement = true;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = true;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = true;
};

template<> struct NodeKindTraits<NodeKind::Port> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = true;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = false;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::Register> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = true;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = false;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::StringReg> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = true;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = true;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = false;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::StructEntry> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = true;
    static constexpr bool hasIntegerElement = true;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = true;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = true;
};

template<> struct NodeKindTraits<NodeKind::StructReg> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = true;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = false;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = true;
    static constexpr bool hasValueElement = false;
    static constexpr bool hasLimitsElement = false;
};

template<> struct NodeKindTraits<NodeKind::SwissKnife> {
    static constexpr bool hasNodeElement = true;
    static constexpr bool hasRegisterElement = false;
    static constexpr bool hasIntegerElement = false;
    static constexpr bool hasFloatElement = false;
    static constexpr bool hasCommandElement = false;
    static constexpr bool hasEnumElement = false;
    static constexpr bool hasConverterElement = false;
    static constexpr bool hasSwissKnifeElement = true;
    static constexpr bool hasPortElement = false;
    static constexpr bool hasStringElement = false;
    static constexpr bool hasBitfieldElement = false;
    static constexpr bool hasValueElement = true;
    static constexpr bool hasLimitsElement = false;
};
