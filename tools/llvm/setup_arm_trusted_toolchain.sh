# ARM TOOLCHAIN SETTINGS FOR TRUSTED CODE
#
# NOTE: you need to 'source' this file, so that the env changes take effect
# inside scons
#

BASE_DIR="$(pwd)/compiler/linux_arm-trusted"

CODE_SOURCERY_PREFIX=${BASE_DIR}/arm-2009q3/bin/arm-none-linux-gnueabi
CODE_SOURCERY_JAIL=${BASE_DIR}/arm-2009q3/arm-none-linux-gnueabi/libc

LD_SCRIPT_TRUSTED=${BASE_DIR}/ld_script_arm_trusted

######################################################################
#
######################################################################

export ARM_CC="${CODE_SOURCERY_PREFIX}-gcc\
                 -Werror\
                 -O2\
                 -pedantic\
                 -std=gnu99\
                 -fdiagnostics-show-option\
                 -I${CODE_SOURCERY_JAIL}/usr/include\
                 -march=armv6\
                 -Wl,-T -Wl,${LD_SCRIPT_TRUSTED}"

export ARM_CXX="${CODE_SOURCERY_PREFIX}-g++\
                 -Werror\
                 -O2\
                 -fdiagnostics-show-option\
                 -I${CODE_SOURCERY_JAIL}/usr/include\
                 -march=armv6\
                 -Wl,-T -Wl,${LD_SCRIPT_TRUSTED}"

export ARM_LD="${CODE_SOURCERY_PREFIX}-ld"
export ARM_LINKFLAGS="-static"
export ARM_LIB_DIR="${CODE_SOURCERY_JAIL}//usr/lib"
export ARM_EMU="${BASE_DIR}/qemu-arm\
                    -cpu cortex-a8\
                    -L ${CODE_SOURCERY_JAIL}"

