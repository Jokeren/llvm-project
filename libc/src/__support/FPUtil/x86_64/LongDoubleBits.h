//===-- Bit representation of x86 long double numbers -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_LONGDOUBLEBITS_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_LONGDOUBLEBITS_H

#include "src/__support/CPP/bit.h"
#include "src/__support/UInt128.h"
#include "src/__support/common.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/properties/architectures.h"

#if !defined(LIBC_TARGET_ARCH_IS_X86)
#error "Invalid include"
#endif

#include "src/__support/FPUtil/FPBits.h"

#include <stdint.h>

namespace LIBC_NAMESPACE {
namespace fputil {

template <> struct FPBits<long double> {
  using UIntType = UInt128;

  static constexpr int EXPONENT_BIAS = 0x3FFF;
  static constexpr int MAX_EXPONENT = 0x7FFF;
  static constexpr UIntType MIN_SUBNORMAL = UIntType(1);
  // Subnormal numbers include the implicit bit in x86 long double formats.
  static constexpr UIntType MAX_SUBNORMAL =
      (UIntType(1) << (MantissaWidth<long double>::VALUE)) - 1;
  static constexpr UIntType MIN_NORMAL =
      (UIntType(3) << MantissaWidth<long double>::VALUE);
  static constexpr UIntType MAX_NORMAL =
      (UIntType(MAX_EXPONENT - 1) << (MantissaWidth<long double>::VALUE + 1)) |
      (UIntType(1) << MantissaWidth<long double>::VALUE) | MAX_SUBNORMAL;

  using FloatProp = FloatProperties<long double>;

  UIntType bits;

  LIBC_INLINE constexpr void set_mantissa(UIntType mantVal) {
    mantVal &= (FloatProp::MANTISSA_MASK);
    bits &= ~(FloatProp::MANTISSA_MASK);
    bits |= mantVal;
  }

  LIBC_INLINE constexpr UIntType get_mantissa() const {
    return bits & FloatProp::MANTISSA_MASK;
  }

  LIBC_INLINE constexpr UIntType get_explicit_mantissa() const {
    // The x86 80 bit float represents the leading digit of the mantissa
    // explicitly. This is the mask for that bit.
    constexpr UIntType EXPLICIT_BIT_MASK =
        (UIntType(1) << FloatProp::MANTISSA_WIDTH);
    return bits & (FloatProp::MANTISSA_MASK | EXPLICIT_BIT_MASK);
  }

  LIBC_INLINE constexpr void set_biased_exponent(UIntType expVal) {
    expVal =
        (expVal << (FloatProp::BIT_WIDTH - 1 - FloatProp::EXPONENT_WIDTH)) &
        FloatProp::EXPONENT_MASK;
    bits &= ~(FloatProp::EXPONENT_MASK);
    bits |= expVal;
  }

  LIBC_INLINE constexpr uint16_t get_biased_exponent() const {
    return uint16_t((bits & FloatProp::EXPONENT_MASK) >>
                    (FloatProp::BIT_WIDTH - 1 - FloatProp::EXPONENT_WIDTH));
  }

  LIBC_INLINE constexpr void set_implicit_bit(bool implicitVal) {
    bits &= ~(UIntType(1) << FloatProp::MANTISSA_WIDTH);
    bits |= (UIntType(implicitVal) << FloatProp::MANTISSA_WIDTH);
  }

  LIBC_INLINE constexpr bool get_implicit_bit() const {
    return bool((bits & (UIntType(1) << FloatProp::MANTISSA_WIDTH)) >>
                FloatProp::MANTISSA_WIDTH);
  }

  LIBC_INLINE constexpr void set_sign(bool signVal) {
    bits &= ~(FloatProp::SIGN_MASK);
    UIntType sign1 = UIntType(signVal) << (FloatProp::BIT_WIDTH - 1);
    bits |= sign1;
  }

  LIBC_INLINE constexpr bool get_sign() const {
    return bool((bits & FloatProp::SIGN_MASK) >> (FloatProp::BIT_WIDTH - 1));
  }

  LIBC_INLINE constexpr FPBits() : bits(0) {}

  template <typename XType,
            cpp::enable_if_t<cpp::is_same_v<long double, XType>, int> = 0>
  LIBC_INLINE constexpr explicit FPBits(XType x)
      : bits(cpp::bit_cast<UIntType>(x)) {
    // bits starts uninitialized, and setting it to a long double only
    // overwrites the first 80 bits. This clears those upper bits.
    bits = bits & ((UIntType(1) << 80) - 1);
  }

  template <typename XType,
            cpp::enable_if_t<cpp::is_same_v<XType, UIntType>, int> = 0>
  LIBC_INLINE constexpr explicit FPBits(XType x) : bits(x) {}

  LIBC_INLINE constexpr operator long double() {
    return cpp::bit_cast<long double>(bits);
  }

  LIBC_INLINE constexpr UIntType uintval() {
    // We zero the padding bits as they can contain garbage.
    return bits & FloatProp::FP_MASK;
  }

  LIBC_INLINE constexpr long double get_val() const {
    return cpp::bit_cast<long double>(bits);
  }

  LIBC_INLINE constexpr int get_exponent() const {
    return int(get_biased_exponent()) - EXPONENT_BIAS;
  }

  // If the number is subnormal, the exponent is treated as if it were the
  // minimum exponent for a normal number. This is to keep continuity between
  // the normal and subnormal ranges, but it causes problems for functions where
  // values are calculated from the exponent, since just subtracting the bias
  // will give a slightly incorrect result. Additionally, zero has an exponent
  // of zero, and that should actually be treated as zero.
  LIBC_INLINE constexpr int get_explicit_exponent() const {
    const int biased_exp = int(get_biased_exponent());
    if (is_zero()) {
      return 0;
    } else if (biased_exp == 0) {
      return 1 - EXPONENT_BIAS;
    } else {
      return biased_exp - EXPONENT_BIAS;
    }
  }

  LIBC_INLINE constexpr bool is_zero() const {
    return get_biased_exponent() == 0 && get_mantissa() == 0 &&
           get_implicit_bit() == 0;
  }

  LIBC_INLINE constexpr bool is_inf() const {
    return get_biased_exponent() == MAX_EXPONENT && get_mantissa() == 0 &&
           get_implicit_bit() == 1;
  }

  LIBC_INLINE constexpr bool is_nan() const {
    if (get_biased_exponent() == MAX_EXPONENT) {
      return (get_implicit_bit() == 0) || get_mantissa() != 0;
    } else if (get_biased_exponent() != 0) {
      return get_implicit_bit() == 0;
    }
    return false;
  }

  LIBC_INLINE constexpr bool is_inf_or_nan() const {
    return (get_biased_exponent() == MAX_EXPONENT) ||
           (get_biased_exponent() != 0 && get_implicit_bit() == 0);
  }

  // Methods below this are used by tests.

  LIBC_INLINE static constexpr long double zero() { return 0.0l; }

  LIBC_INLINE static constexpr long double neg_zero() { return -0.0l; }

  LIBC_INLINE static constexpr long double inf(bool sign = false) {
    FPBits<long double> bits(0.0l);
    bits.set_biased_exponent(MAX_EXPONENT);
    bits.set_implicit_bit(1);
    if (sign) {
      bits.set_sign(true);
    }
    return bits.get_val();
  }

  LIBC_INLINE static constexpr long double neg_inf() { return inf(true); }

  LIBC_INLINE static constexpr long double build_nan(UIntType v) {
    FPBits<long double> bits(0.0l);
    bits.set_biased_exponent(MAX_EXPONENT);
    bits.set_implicit_bit(1);
    bits.set_mantissa(v);
    return bits;
  }

  LIBC_INLINE static constexpr long double build_quiet_nan(UIntType v) {
    return build_nan(FloatProp::QUIET_NAN_MASK | v);
  }

  LIBC_INLINE static constexpr long double min_normal() {
    return FPBits(MIN_NORMAL).get_val();
  }

  LIBC_INLINE static constexpr long double max_normal() {
    return FPBits(MAX_NORMAL).get_val();
  }

  LIBC_INLINE static constexpr long double min_denormal() {
    return FPBits(MIN_SUBNORMAL).get_val();
  }

  LIBC_INLINE static constexpr long double max_denormal() {
    return FPBits(MAX_SUBNORMAL).get_val();
  }

  LIBC_INLINE static constexpr FPBits<long double>
  create_value(bool sign, UIntType biased_exp, UIntType mantissa) {
    FPBits<long double> result;
    result.set_sign(sign);
    result.set_biased_exponent(biased_exp);
    result.set_mantissa(mantissa);
    return result;
  }
};

static_assert(
    sizeof(FPBits<long double>) == sizeof(long double),
    "Internal long double representation does not match the machine format.");

} // namespace fputil
} // namespace LIBC_NAMESPACE

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_X86_64_LONGDOUBLEBITS_H
