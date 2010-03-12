/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// This file takes the compressed arm instruction trie, and generates
// source code that parses ARM instructions.

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_DECODER_GENERATOR_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_DECODER_GENERATOR_H__

#include <list>
#include <map>
#include <set>
#include "native_client/src/trusted/validator_arm/arm_categorize.h"

// This class generates function DecodeNcDecodedInstruction for the given trie,
// in the file with the given name (or to stdout if filename is "-").
// Note: The generated function parses the instruction in the passsed
// NcDeocodedInstruction.
class DecoderGenerator {
 public:
  // Initialize the generator with the given trie, and the corresponding
  // file name to store the generated DecodeNcDecodedInstruction function
  // source code.
  //
  // Note: argument file specifies where the generated instruction decoder
  // should be generated.
  DecoderGenerator(ArmInstructionTrieData* trie_data, FILE* file);

  // Generate source for function DecodeNcDecodedInstruction.
  void Generate();

  ~DecoderGenerator();

 private:

  // Models the fields used to model instructions.
  typedef enum InstValuesField {
    COND_FIELD,              // assumed to be first element in list.
    PREFIX_FIELD,
    OPCODE_FIELD,
    ARG1_FIELD,
    ARG2_FIELD,
    ARG3_FIELD,
    ARG4_FIELD,
    SHIFT_FIELD,
    IMMEDIATE_FIELD,
    INST_VALUES_FIELD_SIZE   // Special end of list marker.
  } InstValuesField;

  // Map form bit patterns (modeling InstValues field values) to
  // the number of times that pattern appears for an ArmInstType.
  typedef std::map<uint32_t, size_t> PatternCountMap;

  // Map from InstValue fields, to the corresponding pattern map for
  // all ARM instructions of a given type.
  typedef std::map<InstValuesField, PatternCountMap*> FieldPatternCountMap;

  // Map from each possible instruction type, to the pattern map for each
  // of the fields associated with the type.
  typedef std::map<ArmInstType, FieldPatternCountMap*> TypePatternCountMap;

  // For a state, the map from (reaching) state to the set of matched bits that
  // reach that (reaching) state.
  typedef std::map<ArmInstructionTrie*, std::set<int> > StateToCaseMap;

  // Returns the corresponding file of the given InstValues.
  uint32_t GetInstValuesField(const InstValues* values, InstValuesField field);

  // Generate the set of ARM instructions.
  void GenerateInstructions();

  // Generates a faster version of NcDecodeMatch (in ncdecode.c), for each
  // instruction type in ArmInstType. These functions will be used to
  // speed up parsing matching instructions.
  void GenerateTypeParsers();

  // Generates code that extracts and tests the correctness of the given
  // InstValues field (name), based on the fact that the field is defined by
  // the bits of the mask. Note: if the mask is zero, the field is just set
  // to zero, and no test is done.
  //
  // Argument add_default is used, along with the mask value to determine if
  // any code is generated. Code is generated only if either (a) mask is
  // non-zero and add_default is false; or (b) mask is zero and add_default
  // is true.
  void GenerateFieldAssignment(
      uint32_t mask,
      const char* field_name,
      bool add_default,
      PatternCountMap* count_map);

  PatternCountMap* GetPatternCountMap(FieldPatternCountMap* field_count_map,
                                      InstValuesField field) {
    if (field_count_map == NULL) {
      return NULL;
    } else {
      return (*field_count_map)[field];
    }
  }

  // Generates code that extracts and tests the correctness of InstValue fields,
  // based on the corresponding sets of masks.
  //
  //
  // Argument add_default is used, along with the mask values to determine if
  // any code is generated. Code is generated for each field only if either
  // (a) the corresponding mask is non-zero and add_default is false; or
  // (b) the corresponding mask is zero and add_default is true.
  void GenerateFieldAssignments(
      const InstValues* masks,
      bool add_default,
      FieldPatternCountMap* field_count_map);

  // For the given state associated with the passed node, loop over the
  // corresponding masked bits, finding all possible successors defined by
  // the mask. If there is only one such possible case, conclude that the
  // maksed bits did not contain any useful matches (for discerning which
  // instruction to match). In such cases, move the mask forward to the
  // next set of bits and try again.
  //
  // Argumments:
  //   node - Trie node defining the state to expand.
  //   state_to_case_map - A map from each possible next state (represented
  //      by the corresponding trie node) to the set of bit sequences that
  //      reach that state for the state defined by node.
  //   matching_node -  The node for which state_to_case_map is defined
  //      (which is either the argument node, or if we skipped nodes, the new
  //      state to match).
  //   mask - The mask to apply to the bits of the corresponding instruction
  //      being matched.
  void ExtractNextMatchingStep(
      ArmInstructionTrie* node,
      StateToCaseMap* state_to_case_map,
      ArmInstructionTrie** matching_node,
      uint32_t* mask);

  // Generate successor states for the extracted next matching step found
  // by method ExtractNextMatchingStep.
  //
  // Arguments:
  //   node - Trie node defining the state to expand.
  //   mask - The mask to apply to the bits matching node.
  //   state_to_case_map - A map from each possible next state (represented
  //      by the corresponding trie node) to the set of bit sequences that
  //      reach that state for the state defined by node.
  void BuildSuccessorStepStates(
      ArmInstructionTrie* node,
      uint32_t mask,
      StateToCaseMap* state_to_case_map);

  // Generate code to match the instructions associated with having
  // matched the given trie node.
  void GenerateInstructionMatch(ArmInstructionTrie* node);

  // If possible, generate code that does the state update by adding
  // the extracted step bits to a base state. Returns true iff able to
  // generate such code.
  //
  // Arguments:
  //   node - Trie node defining the state to expand.
  //   mask - The mask to apply to the bits matching node.
  //   state_to_case_map - A map from each possible next state (represented
  //      by the corresponding trie node) to the set of bit sequences that
  //      reach that state for the state defined by node.
  bool GenerateSimpleStateUpdate(
      ArmInstructionTrie* node,
      uint32_t mask,
      StateToCaseMap* state_to_case_map);

  // Generate if-then-else to recognize the extractee next matching step,
  // if there are only a couple of target states. Returns true if using
  // an if-then-else construct appears appropriate.
  //
  // Arguments:
  //   node - Trie node defining the state to expand.
  //   mask - The mask to apply to the bits matching node.
  //   state_to_case_map - A map from each possible next state (represented
  //      by the corresponding trie node) to the set of bit sequences that
  //      reach that state for the state defined by node.
  bool GenerateNextStepUsingIf(
      ArmInstructionTrie* node,
      uint32_t mask,
      StateToCaseMap* state_to_case_map);

  // Generate switch statement to recognize the extracted next matching step.
  //
  // Arguments:
  //   node - Trie node defining the state to expand.
  //   mask - The mask to apply to the bits matching node.
  //   state_to_case_map - A map from each possible next state (represented
  //      by the corresponding trie node) to the set of bit sequences that
  //      reach that state for the state defined by node.
  void GenerateNextStepUsingSwitch(
      ArmInstructionTrie* node,
      uint32_t mask,
      StateToCaseMap* state_to_case_map);

  // Generates a case entry to recognize the patterns defined by
  // the given trie node.
  void GenerateState(ArmInstructionTrie* node);

  // Moves forward over successor trie nodes of the given trie node,
  // and returns the trie node that matches the given sequence of bits
  // defined by the given mask.
  ArmInstructionTrie* AdvanceOnMask(ArmInstructionTrie* node,
                                    uint32_t mask);

  // Returns the state index for the given trie node, assuming
  // the parser has just matched the given mask.
  // Note: If such a state already exists, it returns the index
  // for the existing state, rather than creating a new state.
  int AddState(ArmInstructionTrie* node, uint32_t mask);

  // Does static analysis on instruction types, defined by
  // the defined set of instructions, and extracts the corresponding field
  // pattern map.
  void ExtractTypePatternCountMap();

  // Fill the pattern count map, based on patterns in instructions of the
  // given type.
  void CollectInstructionTypePatternsForType(
      ArmInstType type,
      FieldPatternCountMap* field_count_map);

  // Compress explicit field patterns into a single entry (under EXPLICIT_MATCH)
  // if more than one explicit value is defined in the map.
  void CompressExplicitFieldPatterns(FieldPatternCountMap* field_count_map);

  // Prints out the given string to the file.
  void PrintString(const char* str);

  // Holds the arm instruction trie that should be used to
  // generate corresponding source code.
  ArmInstructionTrieData* trie_data_;

  // During the call to Generate, points to the file to use.
  FILE* file_;

  // Map from the compressed index of a trie node, to the
  // corresponding state index.
  std::map<int, int> index_to_state_map_;

  // Map from the state index to the corresponding mask that
  // should be applied to that state, if the state is reached.
  std::map<int, uint32_t> index_to_mask_map_;

  // The list of trie nodes, that correspond to states, that still need
  // to be expanded into corresponding source code to recognize the
  // corresponding state.
  std::list<ArmInstructionTrie*> state_worklist_;

  // The type pattern layout information for corresponding fields of each type.
  TypePatternCountMap type_pattern_map;
};

#endif  // NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_DECODER_GENERATOR_H__
