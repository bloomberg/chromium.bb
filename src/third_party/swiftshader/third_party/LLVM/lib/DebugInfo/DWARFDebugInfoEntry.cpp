//===-- DWARFDebugInfoEntry.cpp --------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "DWARFDebugInfoEntry.h"
#include "DWARFCompileUnit.h"
#include "DWARFContext.h"
#include "DWARFDebugAbbrev.h"
#include "DWARFFormValue.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;
using namespace dwarf;

void DWARFDebugInfoEntryMinimal::dump(raw_ostream &OS,
                                      const DWARFCompileUnit *cu,
                                      unsigned recurseDepth,
                                      unsigned indent) const {
  DataExtractor debug_info_data = cu->getDebugInfoExtractor();
  uint32_t offset = Offset;

  if (debug_info_data.isValidOffset(offset)) {
    uint64_t abbrCode = debug_info_data.getULEB128(&offset);

    OS << format("\n0x%8.8x: ", Offset);
    if (abbrCode) {
      if (AbbrevDecl) {
        const char *tagString = TagString(getTag());
        if (tagString)
          OS.indent(indent) << tagString;
        else
          OS.indent(indent) << format("DW_TAG_Unknown_%x", getTag());
        OS << format(" [%u] %c\n", abbrCode,
                     AbbrevDecl->hasChildren() ? '*' : ' ');

        // Dump all data in the .debug_info for the attributes
        const uint32_t numAttributes = AbbrevDecl->getNumAttributes();
        for (uint32_t i = 0; i != numAttributes; ++i) {
          uint16_t attr = AbbrevDecl->getAttrByIndex(i);
          uint16_t form = AbbrevDecl->getFormByIndex(i);
          dumpAttribute(OS, cu, &offset, attr, form, indent);
        }

        const DWARFDebugInfoEntryMinimal *child = getFirstChild();
        if (recurseDepth > 0 && child) {
          while (child) {
            child->dump(OS, cu, recurseDepth-1, indent+2);
            child = child->getSibling();
          }
        }
      } else {
        OS << "Abbreviation code not found in 'debug_abbrev' class for code: "
           << abbrCode << '\n';
      }
    } else {
      OS.indent(indent) << "NULL\n";
    }
  }
}

void DWARFDebugInfoEntryMinimal::dumpAttribute(raw_ostream &OS,
                                               const DWARFCompileUnit *cu,
                                               uint32_t* offset_ptr,
                                               uint16_t attr,
                                               uint16_t form,
                                               unsigned indent) const {
  OS << format("0x%8.8x: ", *offset_ptr);
  OS.indent(indent+2);
  const char *attrString = AttributeString(attr);
  if (attrString)
    OS << attrString;
  else
    OS << format("DW_AT_Unknown_%x", attr);
  const char *formString = FormEncodingString(form);
  if (formString)
    OS << " [" << formString << ']';
  else
    OS << format(" [DW_FORM_Unknown_%x]", form);

  DWARFFormValue formValue(form);

  if (!formValue.extractValue(cu->getDebugInfoExtractor(), offset_ptr, cu))
    return;

  OS << "\t(";
  formValue.dump(OS, cu);
  OS << ")\n";
}

bool DWARFDebugInfoEntryMinimal::extractFast(const DWARFCompileUnit *cu,
                                             const uint8_t *fixed_form_sizes,
                                             uint32_t *offset_ptr) {
  Offset = *offset_ptr;

  DataExtractor debug_info_data = cu->getDebugInfoExtractor();
  uint64_t abbrCode = debug_info_data.getULEB128(offset_ptr);

  assert (fixed_form_sizes); // For best performance this should be specified!

  if (abbrCode) {
    uint32_t offset = *offset_ptr;

    AbbrevDecl = cu->getAbbreviations()->getAbbreviationDeclaration(abbrCode);

    // Skip all data in the .debug_info for the attributes
    const uint32_t numAttributes = AbbrevDecl->getNumAttributes();
    uint32_t i;
    uint16_t form;
    for (i=0; i<numAttributes; ++i) {
      form = AbbrevDecl->getFormByIndex(i);

      const uint8_t fixed_skip_size = fixed_form_sizes[form];
      if (fixed_skip_size)
        offset += fixed_skip_size;
      else {
        bool form_is_indirect = false;
        do {
          form_is_indirect = false;
          uint32_t form_size = 0;
          switch (form) {
          // Blocks if inlined data that have a length field and the data bytes
          // inlined in the .debug_info.
          case DW_FORM_block:
            form_size = debug_info_data.getULEB128(&offset);
            break;
          case DW_FORM_block1:
            form_size = debug_info_data.getU8(&offset);
            break;
          case DW_FORM_block2:
            form_size = debug_info_data.getU16(&offset);
            break;
          case DW_FORM_block4:
            form_size = debug_info_data.getU32(&offset);
            break;

          // Inlined NULL terminated C-strings
          case DW_FORM_string:
            debug_info_data.getCStr(&offset);
            break;

          // Compile unit address sized values
          case DW_FORM_addr:
          case DW_FORM_ref_addr:
            form_size = cu->getAddressByteSize();
            break;

          // 1 byte values
          case DW_FORM_data1:
          case DW_FORM_flag:
          case DW_FORM_ref1:
            form_size = 1;
            break;

          // 2 byte values
          case DW_FORM_data2:
          case DW_FORM_ref2:
            form_size = 2;
            break;

          // 4 byte values
          case DW_FORM_strp:
          case DW_FORM_data4:
          case DW_FORM_ref4:
            form_size = 4;
            break;

          // 8 byte values
          case DW_FORM_data8:
          case DW_FORM_ref8:
            form_size = 8;
            break;

          // signed or unsigned LEB 128 values
          case DW_FORM_sdata:
          case DW_FORM_udata:
          case DW_FORM_ref_udata:
            debug_info_data.getULEB128(&offset);
            break;

          case DW_FORM_indirect:
            form_is_indirect = true;
            form = debug_info_data.getULEB128(&offset);
            break;

          default:
            *offset_ptr = Offset;
            return false;
          }
          offset += form_size;

        } while (form_is_indirect);
      }
    }
    *offset_ptr = offset;
    return true;
  } else {
    AbbrevDecl = NULL;
    return true; // NULL debug tag entry
  }

  return false;
}

bool
DWARFDebugInfoEntryMinimal::extract(const DWARFCompileUnit *cu,
                                    uint32_t *offset_ptr) {
  DataExtractor debug_info_data = cu->getDebugInfoExtractor();
  const uint32_t cu_end_offset = cu->getNextCompileUnitOffset();
  const uint8_t cu_addr_size = cu->getAddressByteSize();
  uint32_t offset = *offset_ptr;
  if ((offset < cu_end_offset) && debug_info_data.isValidOffset(offset)) {
    Offset = offset;

    uint64_t abbrCode = debug_info_data.getULEB128(&offset);

    if (abbrCode) {
      AbbrevDecl = cu->getAbbreviations()->getAbbreviationDeclaration(abbrCode);

      if (AbbrevDecl) {
        uint16_t tag = AbbrevDecl->getTag();

        bool isCompileUnitTag = tag == DW_TAG_compile_unit;
        if(cu && isCompileUnitTag)
          const_cast<DWARFCompileUnit*>(cu)->setBaseAddress(0);

        // Skip all data in the .debug_info for the attributes
        const uint32_t numAttributes = AbbrevDecl->getNumAttributes();
        for (uint32_t i = 0; i != numAttributes; ++i) {
          uint16_t attr = AbbrevDecl->getAttrByIndex(i);
          uint16_t form = AbbrevDecl->getFormByIndex(i);

          if (isCompileUnitTag &&
              ((attr == DW_AT_entry_pc) || (attr == DW_AT_low_pc))) {
            DWARFFormValue form_value(form);
            if (form_value.extractValue(debug_info_data, &offset, cu)) {
              if (attr == DW_AT_low_pc || attr == DW_AT_entry_pc)
                const_cast<DWARFCompileUnit*>(cu)
                  ->setBaseAddress(form_value.getUnsigned());
            }
          } else {
            bool form_is_indirect = false;
            do {
              form_is_indirect = false;
              register uint32_t form_size = 0;
              switch (form) {
              // Blocks if inlined data that have a length field and the data
              // bytes // inlined in the .debug_info
              case DW_FORM_block:
                form_size = debug_info_data.getULEB128(&offset);
                break;
              case DW_FORM_block1:
                form_size = debug_info_data.getU8(&offset);
                break;
              case DW_FORM_block2:
                form_size = debug_info_data.getU16(&offset);
                break;
              case DW_FORM_block4:
                form_size = debug_info_data.getU32(&offset);
                break;

              // Inlined NULL terminated C-strings
              case DW_FORM_string:
                debug_info_data.getCStr(&offset);
                break;

              // Compile unit address sized values
              case DW_FORM_addr:
              case DW_FORM_ref_addr:
                form_size = cu_addr_size;
                break;

              // 1 byte values
              case DW_FORM_data1:
              case DW_FORM_flag:
              case DW_FORM_ref1:
                form_size = 1;
                break;

              // 2 byte values
              case DW_FORM_data2:
              case DW_FORM_ref2:
                form_size = 2;
                break;

                // 4 byte values
              case DW_FORM_strp:
                form_size = 4;
                break;

              case DW_FORM_data4:
              case DW_FORM_ref4:
                form_size = 4;
                break;

              // 8 byte values
              case DW_FORM_data8:
              case DW_FORM_ref8:
                form_size = 8;
                break;

              // signed or unsigned LEB 128 values
              case DW_FORM_sdata:
              case DW_FORM_udata:
              case DW_FORM_ref_udata:
                debug_info_data.getULEB128(&offset);
                break;

              case DW_FORM_indirect:
                form = debug_info_data.getULEB128(&offset);
                form_is_indirect = true;
                break;

              default:
                *offset_ptr = offset;
                return false;
              }

              offset += form_size;
            } while (form_is_indirect);
          }
        }
        *offset_ptr = offset;
        return true;
      }
    } else {
      AbbrevDecl = NULL;
      *offset_ptr = offset;
      return true;    // NULL debug tag entry
    }
  }

  return false;
}

uint32_t
DWARFDebugInfoEntryMinimal::getAttributeValue(const DWARFCompileUnit *cu,
                                              const uint16_t attr,
                                              DWARFFormValue &form_value,
                                              uint32_t *end_attr_offset_ptr)
                                              const {
  if (AbbrevDecl) {
    uint32_t attr_idx = AbbrevDecl->findAttributeIndex(attr);

    if (attr_idx != -1U) {
      uint32_t offset = getOffset();

      DataExtractor debug_info_data = cu->getDebugInfoExtractor();

      // Skip the abbreviation code so we are at the data for the attributes
      debug_info_data.getULEB128(&offset);

      uint32_t idx = 0;
      while (idx < attr_idx)
        DWARFFormValue::skipValue(AbbrevDecl->getFormByIndex(idx++),
                                  debug_info_data, &offset, cu);

      const uint32_t attr_offset = offset;
      form_value = DWARFFormValue(AbbrevDecl->getFormByIndex(idx));
      if (form_value.extractValue(debug_info_data, &offset, cu)) {
        if (end_attr_offset_ptr)
          *end_attr_offset_ptr = offset;
        return attr_offset;
      }
    }
  }

  return 0;
}

const char*
DWARFDebugInfoEntryMinimal::getAttributeValueAsString(
    const DWARFCompileUnit* cu,
    const uint16_t attr,
    const char* fail_value) const {
  DWARFFormValue form_value;
  if (getAttributeValue(cu, attr, form_value)) {
    DataExtractor stringExtractor(cu->getContext().getStringSection(),
        false, 0);
    return form_value.getAsCString(&stringExtractor);
  }
  return fail_value;
}

uint64_t
DWARFDebugInfoEntryMinimal::getAttributeValueAsUnsigned(
    const DWARFCompileUnit* cu,
    const uint16_t attr,
    uint64_t fail_value) const {
  DWARFFormValue form_value;
  if (getAttributeValue(cu, attr, form_value))
      return form_value.getUnsigned();
  return fail_value;
}

int64_t
DWARFDebugInfoEntryMinimal::getAttributeValueAsSigned(
    const DWARFCompileUnit* cu,
    const uint16_t attr,
    int64_t fail_value) const {
  DWARFFormValue form_value;
  if (getAttributeValue(cu, attr, form_value))
      return form_value.getSigned();
  return fail_value;
}

uint64_t
DWARFDebugInfoEntryMinimal::getAttributeValueAsReference(
                                                  const DWARFCompileUnit* cu,
                                                  const uint16_t attr,
                                                  uint64_t fail_value) const {
  DWARFFormValue form_value;
  if (getAttributeValue(cu, attr, form_value))
      return form_value.getReference(cu);
  return fail_value;
}

void
DWARFDebugInfoEntryMinimal::buildAddressRangeTable(const DWARFCompileUnit *cu,
                                               DWARFDebugAranges *debug_aranges)
                                                   const {
  if (AbbrevDecl) {
    uint16_t tag = AbbrevDecl->getTag();
    if (tag == DW_TAG_subprogram) {
      uint64_t hi_pc = -1ULL;
      uint64_t lo_pc = getAttributeValueAsUnsigned(cu, DW_AT_low_pc, -1ULL);
      if (lo_pc != -1ULL)
        hi_pc = getAttributeValueAsUnsigned(cu, DW_AT_high_pc, -1ULL);
      if (hi_pc != -1ULL)
        debug_aranges->appendRange(cu->getOffset(), lo_pc, hi_pc);
    }

    const DWARFDebugInfoEntryMinimal *child = getFirstChild();
    while (child) {
      child->buildAddressRangeTable(cu, debug_aranges);
      child = child->getSibling();
    }
  }
}
