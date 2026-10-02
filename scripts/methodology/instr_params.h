#ifndef INSTR_PARAMS_H
#define INSTR_PARAMS_H

#ifndef INSTR_REPS
#define INSTR_REPS 2048
#endif

#ifndef INSTR_UNROLL
#define INSTR_UNROLL 32
#endif

#define INSTR_INSNS (INSTR_REPS * INSTR_UNROLL)

#endif
