//===- SetTheory.cpp - Generate ordered sets from DAG expressions ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the SetTheory class that computes ordered sets of
// Records from DAG expressions.
//
//===----------------------------------------------------------------------===//

#include "SetTheory.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/Support/Format.h"

using namespace llvm;

// Define the standard operators.
namespace {

typedef SetTheory::RecSet RecSet;
typedef SetTheory::RecVec RecVec;

// (add a, b, ...) Evaluate and union all arguments.
struct AddOp : public SetTheory::Operator {
  void apply(SetTheory &ST, DagInit *Expr, RecSet &Elts) {
    ST.evaluate(Expr->arg_begin(), Expr->arg_end(), Elts);
  }
};

// (sub Add, Sub, ...) Set difference.
struct SubOp : public SetTheory::Operator {
  void apply(SetTheory &ST, DagInit *Expr, RecSet &Elts) {
    if (Expr->arg_size() < 2)
      throw "Set difference needs at least two arguments: " +
        Expr->getAsString();
    RecSet Add, Sub;
    ST.evaluate(*Expr->arg_begin(), Add);
    ST.evaluate(Expr->arg_begin() + 1, Expr->arg_end(), Sub);
    for (RecSet::iterator I = Add.begin(), E = Add.end(); I != E; ++I)
      if (!Sub.count(*I))
        Elts.insert(*I);
  }
};

// (and S1, S2) Set intersection.
struct AndOp : public SetTheory::Operator {
  void apply(SetTheory &ST, DagInit *Expr, RecSet &Elts) {
    if (Expr->arg_size() != 2)
      throw "Set intersection requires two arguments: " + Expr->getAsString();
    RecSet S1, S2;
    ST.evaluate(Expr->arg_begin()[0], S1);
    ST.evaluate(Expr->arg_begin()[1], S2);
    for (RecSet::iterator I = S1.begin(), E = S1.end(); I != E; ++I)
      if (S2.count(*I))
        Elts.insert(*I);
  }
};

// SetIntBinOp - Abstract base class for (Op S, N) operators.
struct SetIntBinOp : public SetTheory::Operator {
  virtual void apply2(SetTheory &ST, DagInit *Expr,
                     RecSet &Set, int64_t N,
                     RecSet &Elts) =0;

  void apply(SetTheory &ST, DagInit *Expr, RecSet &Elts) {
    if (Expr->arg_size() != 2)
      throw "Operator requires (Op Set, Int) arguments: " + Expr->getAsString();
    RecSet Set;
    ST.evaluate(Expr->arg_begin()[0], Set);
    IntInit *II = dynamic_cast<IntInit*>(Expr->arg_begin()[1]);
    if (!II)
      throw "Second argument must be an integer: " + Expr->getAsString();
    apply2(ST, Expr, Set, II->getValue(), Elts);
  }
};

// (shl S, N) Shift left, remove the first N elements.
struct ShlOp : public SetIntBinOp {
  void apply2(SetTheory &ST, DagInit *Expr,
             RecSet &Set, int64_t N,
             RecSet &Elts) {
    if (N < 0)
      throw "Positive shift required: " + Expr->getAsString();
    if (unsigned(N) < Set.size())
      Elts.insert(Set.begin() + N, Set.end());
  }
};

// (trunc S, N) Truncate after the first N elements.
struct TruncOp : public SetIntBinOp {
  void apply2(SetTheory &ST, DagInit *Expr,
             RecSet &Set, int64_t N,
             RecSet &Elts) {
    if (N < 0)
      throw "Positive length required: " + Expr->getAsString();
    if (unsigned(N) > Set.size())
      N = Set.size();
    Elts.insert(Set.begin(), Set.begin() + N);
  }
};

// Left/right rotation.
struct RotOp : public SetIntBinOp {
  const bool Reverse;

  RotOp(bool Rev) : Reverse(Rev) {}

  void apply2(SetTheory &ST, DagInit *Expr,
             RecSet &Set, int64_t N,
             RecSet &Elts) {
    if (Reverse)
      N = -N;
    // N > 0 -> rotate left, N < 0 -> rotate right.
    if (Set.empty())
      return;
    if (N < 0)
      N = Set.size() - (-N % Set.size());
    else
      N %= Set.size();
    Elts.insert(Set.begin() + N, Set.end());
    Elts.insert(Set.begin(), Set.begin() + N);
  }
};

// (decimate S, N) Pick every N'th element of S.
struct DecimateOp : public SetIntBinOp {
  void apply2(SetTheory &ST, DagInit *Expr,
             RecSet &Set, int64_t N,
             RecSet &Elts) {
    if (N <= 0)
      throw "Positive stride required: " + Expr->getAsString();
    for (unsigned I = 0; I < Set.size(); I += N)
      Elts.insert(Set[I]);
  }
};

// (sequence "Format", From, To) Generate a sequence of records by name.
struct SequenceOp : public SetTheory::Operator {
  void apply(SetTheory &ST, DagInit *Expr, RecSet &Elts) {
    if (Expr->arg_size() != 3)
      throw "Bad args to (sequence \"Format\", From, To): " +
        Expr->getAsString();
    std::string Format;
    if (StringInit *SI = dynamic_cast<StringInit*>(Expr->arg_begin()[0]))
      Format = SI->getValue();
    else
      throw "Format must be a string: " + Expr->getAsString();

    int64_t From, To;
    if (IntInit *II = dynamic_cast<IntInit*>(Expr->arg_begin()[1]))
      From = II->getValue();
    else
      throw "From must be an integer: " + Expr->getAsString();
    if (From < 0 || From >= (1 << 30))
      throw "From out of range";

    if (IntInit *II = dynamic_cast<IntInit*>(Expr->arg_begin()[2]))
      To = II->getValue();
    else
      throw "From must be an integer: " + Expr->getAsString();
    if (To < 0 || To >= (1 << 30))
      throw "To out of range";

    RecordKeeper &Records =
      dynamic_cast<DefInit&>(*Expr->getOperator()).getDef()->getRecords();

    int Step = From <= To ? 1 : -1;
    for (To += Step; From != To; From += Step) {
      std::string Name;
      raw_string_ostream OS(Name);
      OS << format(Format.c_str(), unsigned(From));
      Record *Rec = Records.getDef(OS.str());
      if (!Rec)
        throw "No def named '" + Name + "': " + Expr->getAsString();
      // Try to reevaluate Rec in case it is a set.
      if (const RecVec *Result = ST.expand(Rec))
        Elts.insert(Result->begin(), Result->end());
      else
        Elts.insert(Rec);
    }
  }
};

// Expand a Def into a set by evaluating one of its fields.
struct FieldExpander : public SetTheory::Expander {
  StringRef FieldName;

  FieldExpander(StringRef fn) : FieldName(fn) {}

  void expand(SetTheory &ST, Record *Def, RecSet &Elts) {
    ST.evaluate(Def->getValueInit(FieldName), Elts);
  }
};
} // end anonymous namespace

SetTheory::SetTheory() {
  addOperator("add", new AddOp);
  addOperator("sub", new SubOp);
  addOperator("and", new AndOp);
  addOperator("shl", new ShlOp);
  addOperator("trunc", new TruncOp);
  addOperator("rotl", new RotOp(false));
  addOperator("rotr", new RotOp(true));
  addOperator("decimate", new DecimateOp);
  addOperator("sequence", new SequenceOp);
}

void SetTheory::addOperator(StringRef Name, Operator *Op) {
  Operators[Name] = Op;
}

void SetTheory::addExpander(StringRef ClassName, Expander *E) {
  Expanders[ClassName] = E;
}

void SetTheory::addFieldExpander(StringRef ClassName, StringRef FieldName) {
  addExpander(ClassName, new FieldExpander(FieldName));
}

void SetTheory::evaluate(Init *Expr, RecSet &Elts) {
  // A def in a list can be a just an element, or it may expand.
  if (DefInit *Def = dynamic_cast<DefInit*>(Expr)) {
    if (const RecVec *Result = expand(Def->getDef()))
      return Elts.insert(Result->begin(), Result->end());
    Elts.insert(Def->getDef());
    return;
  }

  // Lists simply expand.
  if (ListInit *LI = dynamic_cast<ListInit*>(Expr))
    return evaluate(LI->begin(), LI->end(), Elts);

  // Anything else must be a DAG.
  DagInit *DagExpr = dynamic_cast<DagInit*>(Expr);
  if (!DagExpr)
    throw "Invalid set element: " + Expr->getAsString();
  DefInit *OpInit = dynamic_cast<DefInit*>(DagExpr->getOperator());
  if (!OpInit)
    throw "Bad set expression: " + Expr->getAsString();
  Operator *Op = Operators.lookup(OpInit->getDef()->getName());
  if (!Op)
    throw "Unknown set operator: " + Expr->getAsString();
  Op->apply(*this, DagExpr, Elts);
}

const RecVec *SetTheory::expand(Record *Set) {
  // Check existing entries for Set and return early.
  ExpandMap::iterator I = Expansions.find(Set);
  if (I != Expansions.end())
    return &I->second;

  // This is the first time we see Set. Find a suitable expander.
  try {
    const std::vector<Record*> &SC = Set->getSuperClasses();
    for (unsigned i = 0, e = SC.size(); i != e; ++i)
      if (Expander *Exp = Expanders.lookup(SC[i]->getName())) {
        // This breaks recursive definitions.
        RecVec &EltVec = Expansions[Set];
        RecSet Elts;
        Exp->expand(*this, Set, Elts);
        EltVec.assign(Elts.begin(), Elts.end());
        return &EltVec;
      }
  } catch (const std::string &Error) {
    throw TGError(Set->getLoc(), Error);
  }

  // Set is not expandable.
  return 0;
}

