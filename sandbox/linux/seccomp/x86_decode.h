#ifndef X86_DECODE_H__
#define X86_DECODE_H__
namespace playground {
enum {
    REX_B        = 0x01,
    REX_X        = 0x02,
    REX_R        = 0x04,
    REX_W        = 0x08
};

unsigned short next_inst(const char **ip, bool is64bit, bool *has_prefix = 0,
                         char **rex_ptr    = 0, char **mod_rm_ptr = 0,
                         char **sib_ptr    = 0, bool *is_group   = 0);
} // namespace
#endif // X86_DECODE_H__
