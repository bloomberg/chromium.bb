/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file takes the compressed arm instruction trie, and generates
// source code that parses ARM instructions.

#include <stdio.h>

#include <string>
#include <set>
#include <vector>

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator_arm/decoder_generator.h"
#include "native_client/src/trusted/validator_arm/arm_inst_modeling.h"

#define DEBUGGING 0
#include "native_client/src/trusted/validator_arm/debugging.h"

// Extend the special patterm match symbols with this entry, which denotes
// that one should compare the matched instruction field with the corresponding
// explicit value specified in the expected values for the instruction.
// Note: There is a gap between the last type of bit pattern match defined
// in arm_insts.h (which includes NOT_USED, ANY, NON_ZERO, and CONTAINS_ZERO)
// so that additional patterns can be added without causing conflicts.
#define EXPLICIT_MATCH (-20)

// The number of bits matched at a time (from a state), when
// parsing a given instruction.
static const int kMaskStepBits = 4;

// The number of possible values defined by kMaskStepBits.
static const size_t kMaskStepValues = 16;

// Mask to recognize the "leftmost" sequence of bits in an ARM instruction.
static const uint32_t kInitialMask = 0xF0000000;

// A mask to extract out the (rightmost) kMaskStepBits of a value.
static const int kMaskStep = 0xF;

// A bit marker of the leftmost bit to extract, when processing
// kMaskStepBits bits.
static int kMaskStepBit = 0x1 << (kMaskStepBits - 1);

// Generate a C expression that can be used in a test to match
// the sequence of (consecutive) bits defined by the given mask.
static std::string GenerateValueTest(uint32_t mask) {
  // First find index of rightmost digit in mask.
  char value_string[128];
  int shift = PosOfLowestBitSet(mask);
  if (shift > 0) {
    // The bits aren't the rightmost bits, shift
    // the value accordingly (after masking out the bits).
    snprintf(value_string,
             sizeof(value_string),
             "(value & 0x%08x) >> %d",
             mask,
             shift);
  } else {
    // The bits are the rightmost bits, so just apply
    // the mask to the value.
    snprintf(value_string,
             sizeof(value_string),
             "value & 0x%08x",
             mask);
  }
  std::string test_value(value_string);
  return test_value;
}

DecoderGenerator::DecoderGenerator(ArmInstructionTrieData* trie_data,
                                   FILE* file)
    : trie_data_(trie_data),
      file_(file),
      index_to_state_map_(),
      index_to_mask_map_(),
      state_worklist_(),
      type_pattern_map()
{}

DecoderGenerator::~DecoderGenerator() {
  // For now, don't bother to clean up memory in maps.
}

int DecoderGenerator::AddState(ArmInstructionTrie* node, uint32_t mask) {
  DEBUG(printf("->AddState(%p, %08x)\n", reinterpret_cast<void*>(node), mask));
  std::map<int, int>::const_iterator pos =
      index_to_state_map_.find(node->compressed_index);
  if (pos == index_to_state_map_.end()) {
    // The given node represents a new state, so create it and return
    // the corresponding index.
    int state_index = index_to_state_map_.size();
    index_to_state_map_.insert(
        std::make_pair(node->compressed_index, state_index));
    index_to_mask_map_.insert(
        std::make_pair(state_index, mask));
    state_worklist_.push_back(node);
    DEBUG(printf("new state_index = %d\n", state_index);
          printf("compressed_index = %d\n", node->compressed_index);
          printf("<- added %d\n", state_index));
    return state_index;
  } else {
    // State already exists, return the existing state index.
    DEBUG(printf("<- reuse %d\n", pos->second));
    return pos->second;
  }
}

ArmInstructionTrie* DecoderGenerator::AdvanceOnMask(ArmInstructionTrie* node,
                                                    uint32_t mask) {
  DEBUG(printf("->AdvanceOnMask(%p, %u)\n",
               reinterpret_cast<void*>(node), mask));
  // Walk over each bit defined by the mask.
  int mask_step_bit = kMaskStepBit;
  while (NULL != node && mask_step_bit > 0) {
    // Extract the bit, and try to advance to the corresponding successor.
    bool true_mask = ((mask & mask_step_bit) != 0);
    node = node->successors[true_mask ? 1 : 0];

    // Advance to mask to get the next matching bit.
    mask_step_bit = ((0 == mask_step_bit) ? -1 : (mask_step_bit >> 1));
  }
  DEBUG(printf("<-AdvanceOnMask = %p\n", reinterpret_cast<void*>(node)));
  return node;
}

void DecoderGenerator::BuildSuccessorStepStates(
    ArmInstructionTrie* node,
    uint32_t mask,
    StateToCaseMap* state_to_case_map) {
  if (0 != node->bit_index) {
    // Start by sorting map by values, so that new states are built in order
    // of transition values, not the (semi-random) order of pointer values.
    // This serves two purposes: (1) The generated parser is deterministic;
    // (2) We can can optimize out step branch statements when all successor
    // states follow the pattern of a base state plus the (switch) branch value.
    std::map<int, ArmInstructionTrie*> case_to_state_map;
    for (StateToCaseMap::const_iterator state_iter = state_to_case_map->begin();
         state_iter != state_to_case_map->end();
         ++state_iter) {
      for (std::set<int>::const_iterator case_iter = state_iter->second.begin();
           case_iter != state_iter->second.end();
           ++case_iter) {
        case_to_state_map.insert(std::make_pair(*case_iter, state_iter->first));
      }
    }
    // Now build states matching the case order.
    for (std::map<int, ArmInstructionTrie*>::const_iterator case_iter
             = case_to_state_map.begin();
         case_iter != case_to_state_map.end();
         ++case_iter) {
      AddState(case_iter->second, mask >> kMaskStepBits);
    }
  }
}

void DecoderGenerator::ExtractNextMatchingStep(
    ArmInstructionTrie* node,
    StateToCaseMap* state_to_case_map,
    ArmInstructionTrie** matching_node,
    uint32_t* mask) {
  int state_index = index_to_state_map_[node->compressed_index];
  *mask = index_to_mask_map_[state_index];
  bool found_case_mapping = false;
  DEBUG(printf("-> ExtractNext %p -> %d : 0x%08x\n",
               reinterpret_cast<void*>(node), state_index, *mask));
  while (0 < node->bit_index && !found_case_mapping) {
    // Start by collecting possible valid state transitions.
    DEBUG(printf("-- %p : 0x%08x\n", reinterpret_cast<void*>(node), *mask));
    for (int i = 0; i <= kMaskStep; ++i) {
      ArmInstructionTrie* case_node = AdvanceOnMask(node, i);
      if (NULL != case_node) {
        (*state_to_case_map)[case_node].insert(i);
      }
    }

    // Now see if any useful separation occurred. If not, move forward
    // the step number of bytes and repeat the search.
    found_case_mapping = true;  // assume true unless proven otherwise
    if (1 == state_to_case_map->size()) {
      StateToCaseMap::iterator pos = state_to_case_map->begin();
      ArmInstructionTrie* case_node = pos->first;
      if (kMaskStep == static_cast<int>(pos->second.size()) - 1) {
        found_case_mapping = false;
        *mask >>= kMaskStepBits;
        state_to_case_map->clear();
        node = case_node;
      }
    }
  }

  // Create successor states if defined.
  BuildSuccessorStepStates(node, *mask, state_to_case_map);
  *matching_node = node;
  DEBUG(printf("<- ExtractNext %p 0x%08x\n",
               reinterpret_cast<void*>(*matching_node), *mask));
}

void DecoderGenerator::GenerateInstructionMatch(ArmInstructionTrie* node) {
  // Recognized possible instructions. Try to match the
  // instruction(s) associated with the leaf trie node.
  bool is_first = true;
  ArmInstructionList* next = node->matching_instructions;
  while (NULL != next) {
    const ModeledOpInfo* op =
        modeled_arm_instruction_set.instructions[next->index];
    if (is_first) {
      fprintf(file_,
              "        if (NcDecodeMatch_%s(%d, inst)",
              GetArmInstTypeName(op->inst_type),
              next->index);
      is_first = false;
    } else {
      fprintf(file_,
              " ||\n"
              "            NcDecodeMatch_%s(%d, inst)",
              GetArmInstTypeName(op->inst_type),
              next->index);
    }
    if (NULL == next->next) {
      fprintf(file_,
              ") return;\n"
              "        state = kQuitState;\n"
              "        break;\n");
      break;
    }
    next = next->next;
  }
}

bool DecoderGenerator::GenerateSimpleStateUpdate(
    ArmInstructionTrie* node,
    uint32_t mask,
    StateToCaseMap* state_to_case_map) {
  UNREFERENCED_PARAMETER(node);

  if (state_to_case_map->size() == kMaskStepValues) {
    // true while we can find a common base for each step value
    bool still_good = true;
    // The base state to use for step values.
    int base_state = 0;
    // True on first iteration of following loop
    bool is_first = true;
    // Walk through each successor and see if there is a common base state.
    for (StateToCaseMap::const_iterator state_iter = state_to_case_map->begin();
         still_good && state_iter != state_to_case_map->end();
         ++state_iter) {
      if (state_iter->second.size() == 1) {
        int case_state = AddState(state_iter->first, mask >> kMaskStepBits);
        for (std::set<int>::const_iterator case_iter =
                 state_iter->second.begin();
             still_good && case_iter != state_iter->second.end();
             ++case_iter) {
          int cand_base = case_state - *case_iter;
          if (is_first) {
            // Define the base state.
            base_state = cand_base;
            is_first = false;
          }
          if (base_state != cand_base) {
            // Conflicting base states, can't special case.
            still_good = false;
          }
        }
      }
    }
    if (still_good) {
      // Found special case of common base, define as simple state update
      // instead of switch on possible step values.
      fprintf(file_,
              "        state = %d + (%s);\n"
              "        break;\n",
              base_state,
              GenerateValueTest(mask).c_str());
      return true;
    }
  }
  return false;
}

bool DecoderGenerator::GenerateNextStepUsingIf(
    ArmInstructionTrie* node,
    uint32_t mask,
    StateToCaseMap* state_to_case_map) {
  UNREFERENCED_PARAMETER(node);

  if (state_to_case_map->size() == 2) {
    ArmInstructionTrie* then_node = NULL;
    ArmInstructionTrie* else_node = NULL;
    for (StateToCaseMap::const_iterator iter = state_to_case_map->begin();
         iter != state_to_case_map->end();
         ++iter) {
      if (iter->second.size() == 1) {
        then_node = iter->first;
      } else {
        else_node = iter->first;
      }
    }
    if (NULL != then_node) {
      // Generate if-then-else on extracted cases.
      fprintf(file_,
              "        state = (%d == (%s)) ? %d : %d;\n"
              "        break;\n",
              *((*state_to_case_map)[then_node].begin()),
              GenerateValueTest(mask).c_str(),
              AddState(then_node, mask >> kMaskStepBits),
              AddState(else_node, mask >> kMaskStepBits));
      return true;
    }
  }
  // If reached, we have concluded that an if-then-else is not appropriate.
  return false;
}

void DecoderGenerator::GenerateNextStepUsingSwitch(
    ArmInstructionTrie* node,
    uint32_t mask,
    StateToCaseMap* state_to_case_map) {
  UNREFERENCED_PARAMETER(node);

  // Print out case statement that branches to the corresponding reachable
  // successors, based on the matched bit pattern.
  fprintf(file_, "        switch (%s) {\n", GenerateValueTest(mask).c_str());
  for (StateToCaseMap::const_iterator state_iter = state_to_case_map->begin();
       state_iter != state_to_case_map->end();
       ++state_iter) {
    for (std::set<int>::const_iterator case_iter
             = state_iter->second.begin();
         case_iter != state_iter->second.end();
         ++case_iter) {
      fprintf(file_,
              "          case %d:\n",
              *case_iter);
    }
    fprintf(file_,
            "            state = %d;\n",
            AddState(state_iter->first, mask >> kMaskStepBits));
    fprintf(file_, "            break;\n");
  }
  fprintf(file_,
          "          default:\n"
          "            state = kQuitState;\n"
          "            break;\n"
          "        }\n"
          "        break;\n");
}

void DecoderGenerator::GenerateState(ArmInstructionTrie* node) {
  int state_index = index_to_state_map_[node->compressed_index];
  fprintf(file_, "      case %d:\n", state_index);

  // Buiild a map from each possible successor trie node (i.e. state),
  // to the corresponding sequence of bits that reach that state (from
  // the current state).
  StateToCaseMap state_to_case_map;
  uint32_t mask;
  ArmInstructionTrie* matching_node;
  ExtractNextMatchingStep(node, &state_to_case_map, &matching_node, &mask);

  // Now generate code according to the reached state.
  if (0 == matching_node->bit_index) {
    GenerateInstructionMatch(matching_node);
  } else if (!state_to_case_map.empty()) {
    if (!(GenerateSimpleStateUpdate(matching_node, mask, &state_to_case_map) ||
          GenerateNextStepUsingIf(matching_node, mask, &state_to_case_map))) {
      GenerateNextStepUsingSwitch(matching_node, mask, &state_to_case_map);
    }
  }
}

static bool EndsInOnes(uint32_t mask) {
  return 0 != mask && 0 == (mask & (mask + 1));
}

void DecoderGenerator::GenerateFieldAssignment(
    uint32_t mask,
    const char* field_name,
    bool add_default,
    PatternCountMap* count_map) {
  if (mask) {
    if (!add_default) {
      // Calculate shift
      int shift = PosOfLowestBitSet(mask);
      if (shift) {
        fprintf(file_,
                "  inst->values.%s = ((value & 0x%08x) >> %d);\n",
                field_name, mask, shift);
      } else {
        fprintf(file_,
                "  inst->values.%s = (value & 0x%08x);\n",
                field_name, mask);
      }
      if (count_map != NULL) {
        if (count_map->size() == 1) {
          uint32_t pattern = count_map->begin()->first;
          switch (pattern) {
            case NOT_USED:
              // This shouldn't happen.
              return;
            case ANY:
              return;
            case NON_ZERO:
              fprintf(
                  file_,
                  "  if (0 == inst->values.%s) return FALSE;\n",
                  field_name);
              return;
            case CONTAINS_ZERO:
              if (EndsInOnes(mask >> shift)) {
                fprintf(
                    file_,
                    "  if (0 == ~inst->values.%s) return FALSE;\n",
                    field_name);
              } else {
                fprintf(
                    file_,
                    "  if (0 == (~(inst->values.%s) & 0x%08x)) return FALSE;\n",
                    field_name,
                    (mask >> shift));
              }
              return;
            case EXPLICIT_MATCH:
              fprintf(
                  file_,
                  "  if (inst->values.%s != op->expected_values.%s)\n"
                  "    return FALSE;\n",
                  field_name,
                  field_name);
              return;
            default:
              fprintf(
                  file_,
                  "  if (inst->values.%s != 0x%08x)\n"
                  "    return FALSE;\n",
                  field_name,
                  pattern);
              return;
          }
        }
      }
      // If reached, must add general test.
      fprintf(
          file_,
          "  if (!ValuesDefaultMatch(inst->values.%s,\n"
          "                          op->expected_values.%s,\n"
          "                          0x%08x)) return FALSE;\n",
          field_name,
          field_name,
          (mask >> shift));
    }
  } else if (add_default) {
    fprintf(file_,
            "  inst->values.%s = 0;\n",
            field_name);
  }
}

void DecoderGenerator::GenerateFieldAssignments(
    const InstValues* masks,
    bool add_default,
    FieldPatternCountMap* field_count_map) {
  GenerateFieldAssignment(masks->cond, "cond", add_default,
                          GetPatternCountMap(field_count_map, COND_FIELD));
  GenerateFieldAssignment(masks->prefix, "prefix", add_default,
                          GetPatternCountMap(field_count_map, PREFIX_FIELD));
  GenerateFieldAssignment(masks->opcode, "opcode", add_default,
                          GetPatternCountMap(field_count_map, OPCODE_FIELD));
  GenerateFieldAssignment(masks->arg1, "arg1", add_default,
                          GetPatternCountMap(field_count_map, ARG1_FIELD));
  GenerateFieldAssignment(masks->arg2, "arg2", add_default,
                          GetPatternCountMap(field_count_map, ARG2_FIELD));
  GenerateFieldAssignment(masks->arg3, "arg3", add_default,
                          GetPatternCountMap(field_count_map, ARG3_FIELD));
  GenerateFieldAssignment(masks->arg4, "arg4", add_default,
                          GetPatternCountMap(field_count_map, ARG4_FIELD));
  GenerateFieldAssignment(masks->shift, "shift", add_default,
                          GetPatternCountMap(field_count_map, SHIFT_FIELD));
  GenerateFieldAssignment(masks->immediate, "immediate", add_default,
                          GetPatternCountMap(field_count_map, IMMEDIATE_FIELD));
}

uint32_t DecoderGenerator::GetInstValuesField(
    const InstValues* values, DecoderGenerator::InstValuesField field) {
  switch (field) {
    case COND_FIELD:
      return values->cond;
    case PREFIX_FIELD:
      return values->prefix;
    case OPCODE_FIELD:
      return values->opcode;
    case ARG1_FIELD:
      return values->arg1;
    case ARG2_FIELD:
      return values->arg2;
    case ARG3_FIELD:
      return values->arg3;
    case ARG4_FIELD:
      return values->arg4;
    case SHIFT_FIELD:
      return values->shift;
    case IMMEDIATE_FIELD:
      return values->immediate;
    default:
      // This should not happen.
      return 0;
  }
}

void DecoderGenerator::CollectInstructionTypePatternsForType(
    ArmInstType type,
    FieldPatternCountMap* field_count_map) {
  for (int i = 0; i < modeled_arm_instruction_set.size; ++i) {
    const ModeledOpInfo* op = modeled_arm_instruction_set.instructions[i];
    if (op->inst_type != type) continue;
    for (InstValuesField field = COND_FIELD;
         field < INST_VALUES_FIELD_SIZE;
         field = static_cast<InstValuesField>(field + 1)) {
      PatternCountMap* count_map = NULL;
      FieldPatternCountMap::iterator pos = field_count_map->find(field);
      if (pos == field_count_map->end()) {
        count_map = new PatternCountMap();
        field_count_map->insert(std::make_pair(field, count_map));
      } else {
        count_map = pos->second;
      }
      uint32_t pattern = GetInstValuesField(&(op->expected_values), field);
      PatternCountMap::iterator kind_pos = count_map->find(pattern);
      if (kind_pos == count_map->end()) {
        count_map->insert(std::make_pair(pattern, static_cast<size_t>(1)));
      } else {
        (*count_map)[pattern] = kind_pos->second + 1;
      }
    }
  }
}

void DecoderGenerator::CompressExplicitFieldPatterns(
    FieldPatternCountMap* field_count_map) {
  for (InstValuesField field = COND_FIELD;
       field < INST_VALUES_FIELD_SIZE;
       field = static_cast<InstValuesField>(field + 1)) {
    FieldPatternCountMap::iterator pos = field_count_map->find(field);
    if (pos != field_count_map->end()) {
      PatternCountMap* count_map = pos->second;
      if (NULL != count_map) {
        std::vector<uint32_t> explicit_matches;
        size_t explicit_count = 0;
        for (PatternCountMap::iterator iter = count_map->begin();
             iter != count_map->end();
             ++iter) {
          if (static_cast<int>(iter->first) >= 0) {
            // explicit pattern match.
            explicit_matches.push_back(iter->first);
            explicit_count += iter->second;
          }
        }
        if (explicit_matches.size() > 1) {
          // first remove explicit entries.
          for (std::vector<uint32_t>::const_iterator
                   iter = explicit_matches.begin();
               iter != explicit_matches.end();
               ++iter) {
            count_map->erase(*iter);
          }
          // Now add special explicit match entry.
          count_map->insert(std::make_pair(EXPLICIT_MATCH, explicit_count));
        }
      }
    }
  }
}

void DecoderGenerator::ExtractTypePatternCountMap() {
  // Collect types associated with instructions.
  std::set<ArmInstType> types;
  for (int i = 0; i < modeled_arm_instruction_set.size; ++i) {
    types.insert(modeled_arm_instruction_set.instructions[i]->inst_type);
  }

  // Collect information on each possible type.
  for (std::set<ArmInstType>::const_iterator iter = types.begin();
       iter != types.end();
       ++iter) {
    // Start by filling pattern count map based on patterns in instructions of
    // the specified type.
    ArmInstType type = *iter;
    FieldPatternCountMap* field_count_map = new FieldPatternCountMap();
    type_pattern_map.insert(std::make_pair(type, field_count_map));

    CollectInstructionTypePatternsForType(type, field_count_map);

    CompressExplicitFieldPatterns(field_count_map);
  }
}

void DecoderGenerator::GenerateTypeParsers() {
  for (TypePatternCountMap::iterator iter = type_pattern_map.begin();
       iter != type_pattern_map.end();
       ++iter) {
    ArmInstType type = iter->first;
    const InstValues* masks = GetArmInstMasks(type);
    fprintf(
        file_,
        "Bool NcDecodeMatch_%s(int inst_index,\n"
        "                      NcDecodedInstruction* inst) {\n"
        "  uint32_t value = inst->inst;\n"
        "  const OpInfo* op = arm_instruction_set.instructions[inst_index];\n",
        GetArmInstTypeName(type));
    GenerateFieldAssignments(masks, false, iter->second);
    fprintf(
        file_,
        "  if (NULL != op->check_fcn && !op->check_fcn(inst)) return FALSE;\n");
    GenerateFieldAssignments(masks, true, iter->second);
    fprintf(file_,
            "  inst->matched_inst = op;\n"
            "  return TRUE;\n"
            "}\n\n");
  }
}

void DecoderGenerator::PrintString(const char* s) {
  fputc('"', file_);
  while (*s) {
    char c = *(s++);
    switch (c) {
      case '"':
        fprintf(file_, "\\\"");
        break;
      case '\t':
        fprintf(file_, "\\t");
        break;
      case '\n':
        fprintf(file_, "\\n");
        break;
      default:
        fputc(c, file_);
    }
  }
  fputc('"', file_);
}

/* Print out the given bit pattern to the given file. If zero_fill
 * is true, then add any necessary zero's to print out the corresponding
 * 8 digit (hex) value.
 */
static int PrintInstructionBitPatternCount(FILE* file,
                                           uint32_t pattern,
                                           Bool zero_fill) {
  static const char* kNotUsed = "NOT_USED";
  static const char* kAny = "ANY";
  static const char* kNonZero = "NON_ZERO";
  static const char* kContainsZero = "CONTAINS_ZERO";
  switch (pattern) {
    case NOT_USED:
      fprintf(file, kNotUsed);
      return strlen(kNotUsed);
    case ANY:
      fprintf(file, kAny);
      return strlen(kAny);
    case NON_ZERO:
      fprintf(file, kNonZero);
      return strlen(kNonZero);
    case CONTAINS_ZERO:
      fprintf(file, kContainsZero);
      return strlen(kContainsZero);
    default: {
      char buffer[64];
      char format[64];
      snprintf(format, sizeof(format),
               "%s%s%s", "0x%", (zero_fill ? "08" : ""), PRIx32);
      snprintf(buffer, sizeof(buffer), format, pattern);
      fprintf(file, "%s", buffer);
      return strlen(buffer);
    }
  }
}

/* Print out the given bit pattern to the given file,
 * using a fixed width (so that all patterns use the same number of
 * columns). If zero_fill is true, then pad the bit pattern with zeroes
 * to generate an 8 digit (hex) value.
 */
static void PrintInstructionBitPattern(FILE* file,
                                       uint32_t pattern,
                                       Bool zero_fill) {
  int count = PrintInstructionBitPatternCount(file, pattern, zero_fill);
  for (int i = count; i < 14; ++i) {
    fputc(' ', file);
  }
}

/* Print out the named bit field, of a modeled instruction to the
 * given file. The pattern is the encoding of the pattern of bits
 * that the field can have, while mask is the mask to extract the
 * corresponding bits from the matched instruction.
 */
static void GenerateInstValue(FILE* file,
                              const char* name,
                              uint32_t pattern,
                              uint32_t mask) {
  fprintf(file, "      ");
  PrintInstructionBitPattern(file, pattern, FALSE);
  fprintf(file, ", /* %10s = ", name);
  PrintInstructionBitPattern(file, mask, TRUE);
  fprintf(file, " */\n");
}

void DecoderGenerator::GenerateInstructions() {
  /* Print out the corresponding generated instructions. */
  fprintf(file_, "static const OpInfo kOpInfo[%d] = {\n",
          modeled_arm_instruction_set.size);
  for (int i = 0; i < modeled_arm_instruction_set.size; ++i) {
    const ModeledOpInfo* op = modeled_arm_instruction_set.instructions[i];
    fprintf(file_, "  { ");
    PrintString(op->name);
    fprintf(file_,
            ", %s, &%s,\n     %s, %"PRIdBool", ",
            GetArmInstKindName(op->inst_kind),
            op->inst_access,
            GetArmInstTypeName(op->inst_type),
            op->arm_safe);
    PrintString(op->describe_format);
    fprintf(file_, ", %d, %s, {\n", op->num_bytes, op->check_fcn);

    const InstValues* masks = GetArmInstMasks(op->inst_type);
    GenerateInstValue(file_, "cond",
                      op->expected_values.cond, masks->cond);
    GenerateInstValue(file_, "prefix",
                      op->expected_values.prefix, masks->prefix);
    GenerateInstValue(file_, "opcode",
                      op->expected_values.opcode, masks->opcode);
    GenerateInstValue(file_, "arg1",
                      op->expected_values.arg1, masks->arg1);
    GenerateInstValue(file_, "arg2",
                      op->expected_values.arg2, masks->arg2);
    GenerateInstValue(file_, "arg3",
                      op->expected_values.arg3, masks->arg3);
    GenerateInstValue(file_, "arg4",
                      op->expected_values.arg4, masks->arg4);
    GenerateInstValue(file_, "shift",
                      op->expected_values.shift, masks->shift);
    GenerateInstValue(file_, "immediate",
                      op->expected_values.immediate, masks->immediate);
    fprintf(file_, "    }\n  },\n");
  }
  fprintf(file_, "};\n\n");

  /* Print out the data structure holding the generated instructions. */
  fprintf(file_, "ArmInstructionSet arm_instruction_set = {\n");
  fprintf(file_, "  %d,\n", modeled_arm_instruction_set.size);
  fprintf(file_, "  {\n");
  for (int i = 0; i < modeled_arm_instruction_set.size; ++i) {
    fprintf(file_, "    kOpInfo + %d,\n", i);
  }
  fprintf(file_, "  }\n");
  fprintf(file_, "};\n\n");
}

void DecoderGenerator::Generate() {
  ExtractTypePatternCountMap();

  // Print file preamble.
  fprintf(file_,
          "/**** DO NOT EDIT -- Automatically generated!!! ****/\n"
          "\n"
          "#include \"native_client/src/trusted/validator_arm/arm_insts_rt.h\"\n"
          "\n");

  GenerateInstructions();
  GenerateTypeParsers();

  // Print parser method preamble.
  fprintf(file_,
          "static int kQuitState = -1;\n"
          "\n"
          "void DecodeNcDecodedInstruction(NcDecodedInstruction* inst) {\n"
          "  int state = 0;\n"
          "  uint32_t value = inst->inst;\n"
          "  while(state >= 0) {\n"
          "    switch (state) {\n");

  // Generate state machine to parse instruction.
  AddState(trie_data_->root, kInitialMask);
  while (!state_worklist_.empty()) {
    ArmInstructionTrie* node = state_worklist_.front();
    state_worklist_.pop_front();
    GenerateState(node);
  }

  // Print parser method postamble.
  fprintf(file_,
          "      default:\n"
          "        state = kQuitState;\n"
          "        break;\n"
          "    }\n"
          "  }\n"
          "  /* No instruction matched, assume undefined. */\n");
  GenerateFieldAssignments(GetArmInstMasks(ARM_UNDEFINED), true, NULL);
  fprintf(file_,
          "  inst->matched_inst = &kUndefinedArmOp;\n"
          "  return;\n"
          "}\n");
}
