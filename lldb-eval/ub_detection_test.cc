// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <ostream>
#include <string>

#include "lldb-eval/context.h"
#include "lldb-eval/eval.h"
#include "lldb-eval/parser.h"
#include "lldb-eval/runner.h"
#include "lldb/API/SBDebugger.h"
#include "lldb/API/SBError.h"
#include "lldb/API/SBFrame.h"
#include "lldb/API/SBProcess.h"
#include "lldb/API/SBTarget.h"
#include "lldb/API/SBThread.h"
#include "tools/cpp/runfiles/runfiles.h"

// DISALLOW_COPY_AND_ASSIGN is also defined in
// lldb/lldb-defines.h
#undef DISALLOW_COPY_AND_ASSIGN
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using bazel::tools::cpp::runfiles::Runfiles;

using lldb_eval::UbStatus;

namespace lldb_eval {
std::ostream& operator<<(std::ostream& os, UbStatus status) {
  switch (status) {
    case UbStatus::kOk:
      return os << "UbStatus::kOk";
    case UbStatus::kDivisionByZero:
      return os << "UbStatus::kDivisionByZero";
    case UbStatus::kDivisionByMinusOne:
      return os << "UbStatus::kDivisionByMinusOne";
    case UbStatus::kInvalidCast:
      return os << "UbStatus::kInvalidCast";
    case UbStatus::kInvalidShift:
      return os << "UbStatus::kInvalidShift";
    case UbStatus::kNullptrArithmetic:
      return os << "UbStatus::kNullptrArithmetic";
    case UbStatus::kInvalidPtrDiff:
      return os << "UbStatus::kInvalidPtrDiff";

    default:
      assert(false && "Did you introduce a new UbStatus?");
      return os << "invalid UbStatus";
  }
}
}  // namespace lldb_eval

class UbDetectionTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    runfiles_ = Runfiles::CreateForTest();
    lldb_eval::SetupLLDBServerEnv(*runfiles_);
    lldb::SBDebugger::Initialize();
  }

  static void TearDownTestSuite() {
    lldb::SBDebugger::Terminate();
    delete runfiles_;
    runfiles_ = nullptr;
  }

  void SetUp() {
    std::string break_line = "// BREAK HERE";

    auto binary_path =
        runfiles_->Rlocation("lldb_eval/testdata/ub_detection_binary");
    auto source_path =
        runfiles_->Rlocation("lldb_eval/testdata/ub_detection_binary.cc");

    debugger_ = lldb::SBDebugger::Create(false);
    process_ = lldb_eval::LaunchTestProgram(debugger_, source_path, binary_path,
                                            break_line);
    frame_ = process_.GetSelectedThread().GetSelectedFrame();
  }

  void TearDown() {
    process_.Destroy();
    lldb::SBDebugger::Destroy(debugger_);
  }

  UbStatus GetUbStatus(const std::string& expr) {
    auto sm = lldb_eval::SourceManager::Create(expr);
    auto ctx = lldb_eval::Context::Create(sm, frame_);

    lldb_eval::Error err;
    lldb_eval::Parser p(ctx);
    lldb_eval::ExprResult tree = p.Run(err);

    assert(!err && "Error while parsing expression!");

    lldb_eval::Interpreter eval(process_.GetTarget(), sm);
    lldb_eval::Value ret = eval.Eval(tree.get(), err);

    assert(!err && "Error while evaluating expression!");

    return err.ub_status();
  }

 protected:
  lldb::SBDebugger debugger_;
  lldb::SBProcess process_;
  lldb::SBFrame frame_;

  static Runfiles* runfiles_;
};

Runfiles* UbDetectionTest::runfiles_ = nullptr;

TEST_F(UbDetectionTest, TestDivisionByZero) {
  EXPECT_EQ(GetUbStatus("1 / 0"), UbStatus::kDivisionByZero);
  EXPECT_EQ(GetUbStatus("1 % 0"), UbStatus::kDivisionByZero);
  EXPECT_EQ(GetUbStatus("1 / (i - 1)"), UbStatus::kDivisionByZero);

  // Division with floating point zero yields infinity.
  EXPECT_EQ(GetUbStatus("1 / 0.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 / 0.f"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 / -0.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 / -0.f"), UbStatus::kOk);
  // Zero that is implicitly converted to floating point type.
  EXPECT_EQ(GetUbStatus("1.0 / 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1.f / 0"), UbStatus::kOk);

  EXPECT_EQ(GetUbStatus("0 / 1"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("0 % 1"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 + 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 - 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 * 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 & 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 | 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 ^ 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 << 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 >> 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 || 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 && 0"), UbStatus::kOk);
}

TEST_F(UbDetectionTest, TestDivisionByMinusOne) {
  EXPECT_EQ(GetUbStatus("int_min / -1"), UbStatus::kDivisionByMinusOne);
  EXPECT_EQ(GetUbStatus("long_min / -1"), UbStatus::kDivisionByMinusOne);
  EXPECT_EQ(GetUbStatus("llong_min / -1"), UbStatus::kDivisionByMinusOne);
  EXPECT_EQ(GetUbStatus("myint_min / -1"), UbStatus::kDivisionByMinusOne);
  EXPECT_EQ(GetUbStatus("int_min % -1"), UbStatus::kDivisionByMinusOne);
  EXPECT_EQ(GetUbStatus("long_min % -1"), UbStatus::kDivisionByMinusOne);
  EXPECT_EQ(GetUbStatus("llong_min % -1"), UbStatus::kDivisionByMinusOne);
  EXPECT_EQ(GetUbStatus("myint_min % -1"), UbStatus::kDivisionByMinusOne);

  EXPECT_EQ(GetUbStatus("int_min / -2"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(int_min + 1) / -1"), UbStatus::kOk);
  // Note that 2147483648 is an unsigned integer.
  EXPECT_EQ(GetUbStatus("-2147483648 / -1"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(-2147483647 -1) / -1"),
            UbStatus::kDivisionByMinusOne);

  EXPECT_EQ(GetUbStatus("(unsigned int)int_min / -1"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("int_min / 4294967295U"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("llong_min / 18446744073709551615LLU"), UbStatus::kOk);
}

TEST_F(UbDetectionTest, TestInvalidCast) {
  EXPECT_EQ(GetUbStatus("(int)2147483647.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(int)2147483648.0"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)-2147483648.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(int)-2147483649.0"), UbStatus::kInvalidCast);
  // With floats.
  EXPECT_EQ(GetUbStatus("(int)2147483500.f"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(int)2147483800.f"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)-2147483500.f"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(int)-2147483800.f"), UbStatus::kInvalidCast);
  // Unsigned.
  EXPECT_EQ(GetUbStatus("(unsigned int)4294967295.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(unsigned int)4294967296.0"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(unsigned int)0.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(unsigned int)-1.0"), UbStatus::kInvalidCast);
  // Values that has to be truncated.
  EXPECT_EQ(GetUbStatus("(unsigned int)4294967295.8"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(unsigned int)-0.1"), UbStatus::kOk);

  EXPECT_EQ(GetUbStatus("(signed char)127.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(signed char)128.0"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(signed char)-128.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(signed char)-129.0"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(unsigned char)255.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(unsigned char)256.0"), UbStatus::kInvalidCast);

  EXPECT_EQ(GetUbStatus("(short)32767.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(short)32768.0"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(short)-32768.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(short)-32769.0"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(unsigned short)65535.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(unsigned short)65536.0"), UbStatus::kInvalidCast);

  EXPECT_EQ(GetUbStatus("(long long)9.223372036854775E+18"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(long long)9.223372036854777E+18"),
            UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(long long)-9.223372036854775E+18"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(long long)-9.223372036854777E+18"),
            UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(unsigned long long)1.844674407370955E+19"),
            UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(unsigned long long)1.844674407370957E+19"),
            UbStatus::kInvalidCast);

  EXPECT_EQ(GetUbStatus("(int)(1 / 0.0)"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)(1 / 0.f)"), UbStatus::kInvalidCast);

  // Corner cases.
  EXPECT_EQ(GetUbStatus("(int)finf"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)-finf"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)fnan"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)fsnan"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)fdenorm"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(int)fmax"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)-fmax"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(unsigned int)-fdenorm"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(unsigned long long)fmax"), UbStatus::kInvalidCast);

  EXPECT_EQ(GetUbStatus("(unsigned int)finf"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(unsigned int)-finf"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(unsigned int)fnan"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(unsigned int)fsnan"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(unsigned int)fdenorm"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(unsigned int)fmax"), UbStatus::kInvalidCast);

  EXPECT_EQ(GetUbStatus("(int)dinf"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)-dinf"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)dnan"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)dsnan"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(int)ddenorm"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(int)dmax"), UbStatus::kInvalidCast);

  // Test with ScopedEnum (underlying type is int).
  EXPECT_EQ(GetUbStatus("(ScopedEnum)2147483647.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)2147483648.0"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)-2147483648.0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)-2147483649.0"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)-2147483648.5"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)2147483500.f"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)2147483900.f"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)fnan"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)fsnan"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)fmax"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)finf"), UbStatus::kInvalidCast);
  EXPECT_EQ(GetUbStatus("(ScopedEnum)fdenorm"), UbStatus::kOk);

  // TODO: Add tests with enums of custom underlying type.
}

TEST_F(UbDetectionTest, TestNullptrArithmetic) {
  EXPECT_EQ(GetUbStatus("(int*)0 + 4"), UbStatus::kNullptrArithmetic);
  EXPECT_EQ(GetUbStatus("(int*)0 + (-4)"), UbStatus::kNullptrArithmetic);
  EXPECT_EQ(GetUbStatus("(int*)0 + 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(int*)4 + (-4)"), UbStatus::kOk);

  // Subtraction didn't cause mismatches so far.
  EXPECT_EQ(GetUbStatus("(int*)0 - 4"), UbStatus::kOk);

  EXPECT_EQ(GetUbStatus("inp + 4"), UbStatus::kNullptrArithmetic);
  EXPECT_EQ(GetUbStatus("inp + (-4)"), UbStatus::kNullptrArithmetic);

  EXPECT_EQ(GetUbStatus("(int*)nullptr + (-4)"), UbStatus::kNullptrArithmetic);
  EXPECT_EQ(GetUbStatus("(int*)null_ptr + (-4)"), UbStatus::kNullptrArithmetic);
  EXPECT_EQ(GetUbStatus("(int*)null_ptr_ref + (-4)"),
            UbStatus::kNullptrArithmetic);
  EXPECT_EQ(GetUbStatus("(int*)*null_ptr_addr + (-4)"),
            UbStatus::kNullptrArithmetic);
  EXPECT_EQ(GetUbStatus("(int*)null_ptr_addr[0] + (-4)"),
            UbStatus::kNullptrArithmetic);
  EXPECT_EQ(GetUbStatus("(int*)s.null_field + (-4)"),
            UbStatus::kNullptrArithmetic);
  EXPECT_EQ(GetUbStatus("(int*)(&s)->null_field + (-4)"),
            UbStatus::kNullptrArithmetic);

  // Multiple casts.
  EXPECT_EQ(GetUbStatus("(int*)(char*)0 + (-4)"), UbStatus::kNullptrArithmetic);

  EXPECT_EQ(GetUbStatus("&inp[1]"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("&inp[-1]"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("&inp[0]"), UbStatus::kOk);
}

TEST_F(UbDetectionTest, TestInvalidShift) {
  // Left shift.
  EXPECT_EQ(GetUbStatus("1 << 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 << 31"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 << -1"), UbStatus::kInvalidShift);
  EXPECT_EQ(GetUbStatus("1 << 32"), UbStatus::kInvalidShift);

  // Right shift.
  EXPECT_EQ(GetUbStatus("1 >> 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 >> 31"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1 >> -1"), UbStatus::kInvalidShift);
  EXPECT_EQ(GetUbStatus("1 >> 32"), UbStatus::kInvalidShift);

  // Left operand has different size.
  EXPECT_EQ(GetUbStatus("1LL << 0"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1LL << 63"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("1LL << -1"), UbStatus::kInvalidShift);
  EXPECT_EQ(GetUbStatus("1LL << 64"), UbStatus::kInvalidShift);

  // Try different values of the left operand.
  EXPECT_EQ(GetUbStatus("10000 << 30"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("-10000 << 30"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("-1LL >> 10"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("100U << 30"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("100ULL << 60"), UbStatus::kOk);

  // `char` type is implicitly converted to `int`.
  EXPECT_EQ(GetUbStatus("(char)1 << 31LL"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(char)1 << 32LL"), UbStatus::kInvalidShift);
}

TEST_F(UbDetectionTest, TestInvalidPtrDiff) {
  EXPECT_EQ(GetUbStatus("(int*)4 - (int*)8"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(int*)4 - (int*)10"), UbStatus::kInvalidPtrDiff);
  EXPECT_EQ(GetUbStatus("(short*)4 - (short*)10"), UbStatus::kOk);
  EXPECT_EQ(GetUbStatus("(short*)4 - (short*)5"), UbStatus::kInvalidPtrDiff);

  // The following expressions may not be well defined, but didn't cause
  // mismatches between LLDB and lldb-eval, so we allow it for now.

  // Pointers that are not correctly aligned.
  EXPECT_EQ(GetUbStatus("(int*)3 - (int*)7"), UbStatus::kOk);
  // Invalid difference greater than 0.
  EXPECT_EQ(GetUbStatus("(int*)7 - (int*)6"), UbStatus::kOk);
}

// TODO: Add tests with composite assignments (e.g. `i /= 0`, `i -= fmax`).
