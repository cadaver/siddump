#include <stdio.h>
#include <stdlib.h>

#define FN 0x80
#define FV 0x40
#define FB 0x10
#define FD 0x08
#define FI 0x04
#define FZ 0x02
#define FC 0x01

#define MEM(address) (mem[address])
#define LO() (MEM(pc))
#define HI() (MEM(pc+1))
#define FETCH() (MEM(pc++))
#define SETPC(newpc) (pc = (newpc))
#define PUSH(data) (MEM(0x100 + (sp--)) = (data))
#define POP() (MEM(0x100 + (++sp)))

#define IMMEDIATE() (LO())
#define ABSOLUTE() (LO() | (HI() << 8))
#define ABSOLUTEX() (((LO() | (HI() << 8)) + x) & 0xffff)
#define ABSOLUTEY() (((LO() | (HI() << 8)) + y) & 0xffff)
#define ZEROPAGE() (LO() & 0xff)
#define ZEROPAGEX() ((LO() + x) & 0xff)
#define ZEROPAGEY() ((LO() + y) & 0xff)
#define INDIRECTX() (MEM((LO() + x) & 0xff) | (MEM((LO() + x + 1) & 0xff) << 8))
#define INDIRECTY() (((MEM(LO()) | (MEM((LO() + 1) & 0xff) << 8)) + y) & 0xffff)
#define INDIRECTZP() (((MEM(LO()) | (MEM((LO() + 1) & 0xff) << 8)) + 0) & 0xffff)

#define WRITE(address)                  \
{                                       \
  /* cpuwritemap[(address) >> 6] = 1; */  \
}

#define EVALPAGECROSSING(baseaddr, realaddr) ((((baseaddr) ^ (realaddr)) & 0xff00) ? 1 : 0)
#define EVALPAGECROSSING_ABSOLUTEX() (EVALPAGECROSSING(ABSOLUTE(), ABSOLUTEX()))
#define EVALPAGECROSSING_ABSOLUTEY() (EVALPAGECROSSING(ABSOLUTE(), ABSOLUTEY()))
#define EVALPAGECROSSING_INDIRECTY() (EVALPAGECROSSING(INDIRECTZP(), INDIRECTY()))

#define BRANCH()                                          \
{                                                         \
  ++cpucycles;                                            \
  temp = FETCH();                                         \
  if (temp < 0x80)                                        \
  {                                                       \
    cpucycles += EVALPAGECROSSING(pc, pc + temp);         \
    SETPC(pc + temp);                                     \
  }                                                       \
  else                                                    \
  {                                                       \
    cpucycles += EVALPAGECROSSING(pc, pc + temp - 0x100); \
    SETPC(pc + temp - 0x100);                             \
  }                                                       \
}

#define SETFLAGS(data)                  \
{                                       \
  if (!(data))                          \
    flags = (flags & ~FN) | FZ;         \
  else                                  \
    flags = (flags & ~(FN|FZ)) |        \
    ((data) & FN);                      \
}

#define ASSIGNSETFLAGS(dest, data)      \
{                                       \
  dest = data;                          \
  if (!dest)                            \
    flags = (flags & ~FN) | FZ;         \
  else                                  \
    flags = (flags & ~(FN|FZ)) |        \
    (dest & FN);                        \
}

#define ADC(data)                                                        \
{                                                                        \
    unsigned tempval = data;                                             \
                                                                         \
    if (flags & FD)                                                      \
    {                                                                    \
        temp = (a & 0xf) + (tempval & 0xf) + (flags & FC);               \
        if (temp > 0x9)                                                  \
            temp += 0x6;                                                 \
        if (temp <= 0x0f)                                                \
            temp = (temp & 0xf) + (a & 0xf0) + (tempval & 0xf0);         \
        else                                                             \
            temp = (temp & 0xf) + (a & 0xf0) + (tempval & 0xf0) + 0x10;  \
        if (!((a + tempval + (flags & FC)) & 0xff))                      \
            flags |= FZ;                                                 \
        else                                                             \
            flags &= ~FZ;                                                \
        if (temp & 0x80)                                                 \
            flags |= FN;                                                 \
        else                                                             \
            flags &= ~FN;                                                \
        if (((a ^ temp) & 0x80) && !((a ^ tempval) & 0x80))              \
            flags |= FV;                                                 \
        else                                                             \
            flags &= ~FV;                                                \
        if ((temp & 0x1f0) > 0x90) temp += 0x60;                         \
        if ((temp & 0xff0) > 0xf0)                                       \
            flags |= FC;                                                 \
        else                                                             \
            flags &= ~FC;                                                \
    }                                                                    \
    else                                                                 \
    {                                                                    \
        temp = tempval + a + (flags & FC);                               \
        SETFLAGS(temp & 0xff);                                           \
        if (!((a ^ tempval) & 0x80) && ((a ^ temp) & 0x80))              \
            flags |= FV;                                                 \
        else                                                             \
            flags &= ~FV;                                                \
        if (temp > 0xff)                                                 \
            flags |= FC;                                                 \
        else                                                             \
            flags &= ~FC;                                                \
    }                                                                    \
    a = temp;                                                            \
}

#define SBC(data)                                                        \
{                                                                        \
    unsigned tempval = data;                                             \
    temp = a - tempval - ((flags & FC) ^ FC);                            \
                                                                         \
    if (flags & FD)                                                      \
    {                                                                    \
        unsigned tempval2;                                               \
        tempval2 = (a & 0xf) - (tempval & 0xf) - ((flags & FC) ^ FC);    \
        if (tempval2 & 0x10)                                             \
            tempval2 = ((tempval2 - 6) & 0xf) | ((a & 0xf0) - (tempval   \
            & 0xf0) - 0x10);                                             \
        else                                                             \
            tempval2 = (tempval2 & 0xf) | ((a & 0xf0) - (tempval         \
            & 0xf0));                                                    \
        if (tempval2 & 0x100)                                            \
            tempval2 -= 0x60;                                            \
        if (temp < 0x100)                                                \
            flags |= FC;                                                 \
        else                                                             \
            flags &= ~FC;                                                \
        SETFLAGS(temp & 0xff);                                           \
        if (((a ^ temp) & 0x80) && ((a ^ tempval) & 0x80))               \
            flags |= FV;                                                 \
        else                                                             \
            flags &= ~FV;                                                \
        a = tempval2;                                                    \
    }                                                                    \
    else                                                                 \
    {                                                                    \
        SETFLAGS(temp & 0xff);                                           \
        if (temp < 0x100)                                                \
            flags |= FC;                                                 \
        else                                                             \
            flags &= ~FC;                                                \
        if (((a ^ temp) & 0x80) && ((a ^ tempval) & 0x80))               \
            flags |= FV;                                                 \
        else                                                             \
            flags &= ~FV;                                                \
        a = temp;                                                        \
    }                                                                    \
}

#define CMP(src, data)                  \
{                                       \
  temp = (src - data) & 0xff;           \
                                        \
  flags = (flags & ~(FC|FN|FZ)) |       \
          (temp & FN);                  \
                                        \
  if (!temp) flags |= FZ;               \
  if (src >= data) flags |= FC;         \
}

#define ASL(data)                       \
{                                       \
  temp = data;                          \
  temp <<= 1;                           \
  if (temp & 0x100) flags |= FC;        \
  else flags &= ~FC;                    \
  ASSIGNSETFLAGS(data, temp);           \
}

#define LSR(data)                       \
{                                       \
  temp = data;                          \
  if (temp & 1) flags |= FC;            \
  else flags &= ~FC;                    \
  temp >>= 1;                           \
  ASSIGNSETFLAGS(data, temp);           \
}

#define ROL(data)                       \
{                                       \
  temp = data;                          \
  temp <<= 1;                           \
  if (flags & FC) temp |= 1;            \
  if (temp & 0x100) flags |= FC;        \
  else flags &= ~FC;                    \
  ASSIGNSETFLAGS(data, temp);           \
}

#define ROR(data)                       \
{                                       \
  temp = data;                          \
  if (flags & FC) temp |= 0x100;        \
  if (temp & 1) flags |= FC;            \
  else flags &= ~FC;                    \
  temp >>= 1;                           \
  ASSIGNSETFLAGS(data, temp);           \
}

#define DEC(data)                       \
{                                       \
  temp = data - 1;                      \
  ASSIGNSETFLAGS(data, temp);           \
}

#define INC(data)                       \
{                                       \
  temp = data + 1;                      \
  ASSIGNSETFLAGS(data, temp);           \
}

#define EOR(data)                       \
{                                       \
  a ^= data;                            \
  SETFLAGS(a);                          \
}

#define ORA(data)                       \
{                                       \
  a |= data;                            \
  SETFLAGS(a);                          \
}

#define AND(data)                       \
{                                       \
  a &= data;                            \
  SETFLAGS(a)                           \
}

#define BIT(data)                       \
{                                       \
  flags = (flags & ~(FN|FV)) |          \
          (data & (FN|FV));             \
  if (!(data & a)) flags |= FZ;         \
  else flags &= ~FZ;                    \
}

void initcpu(unsigned short newpc, unsigned char newa, unsigned char newx, unsigned char newy);
int runcpu(void);
void setpc(unsigned short newpc);

unsigned short pc;
unsigned char a;
unsigned char x;
unsigned char y;
unsigned char flags;
unsigned char sp;
unsigned char mem[0x10000];
unsigned int cpucycles;

static const int cpucycles_table[] = 
{
  7,  6,  0,  8,  3,  3,  5,  5,  3,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  6,  6,  0,  8,  3,  3,  5,  5,  4,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  6,  6,  0,  8,  3,  3,  5,  5,  3,  2,  2,  2,  3,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  6,  6,  0,  8,  3,  3,  5,  5,  4,  2,  2,  2,  5,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  2,  6,  2,  6,  3,  3,  3,  3,  2,  2,  2,  2,  4,  4,  4,  4, 
  2,  6,  0,  6,  4,  4,  4,  4,  2,  5,  2,  5,  5,  5,  5,  5, 
  2,  6,  2,  6,  3,  3,  3,  3,  2,  2,  2,  2,  4,  4,  4,  4, 
  2,  5,  0,  5,  4,  4,  4,  4,  2,  4,  2,  4,  4,  4,  4,  4, 
  2,  6,  2,  8,  3,  3,  5,  5,  2,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7, 
  2,  6,  2,  8,  3,  3,  5,  5,  2,  2,  2,  2,  4,  4,  6,  6, 
  2,  5,  0,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  7
};

void initcpu(unsigned short newpc, unsigned char newa, unsigned char newx, unsigned char newy)
{
  pc = newpc;
  a = newa;
  x = newx;
  y = newy;
  flags = 0;
  sp = 0xff;
  cpucycles = 0;
}

int runcpu(void)
{
  unsigned temp;

  unsigned char op = FETCH();
  /* printf("PC: %04x OP: %02x A:%02x X:%02x Y:%02x\n", pc-1, op, a, x, y); */
  cpucycles += cpucycles_table[op];
  switch(op)
  {
    case 0xa7:
    ASSIGNSETFLAGS(a, MEM(ZEROPAGE()));
    x = a;
    pc++;
    break;

    case 0xb7:
    ASSIGNSETFLAGS(a, MEM(ZEROPAGEY()));
    x = a;
    pc++;
    break;

    case 0xaf:
    ASSIGNSETFLAGS(a, MEM(ABSOLUTE()));
    x = a;
    pc += 2;
    break;

    case 0xa3:
    ASSIGNSETFLAGS(a, MEM(INDIRECTX()));
    x = a;
    pc++;
    break;

    case 0xb3:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    ASSIGNSETFLAGS(a, MEM(INDIRECTY()));
    x = a;
    pc++;
    break;
    
    case 0x1a:
    case 0x3a:
    case 0x5a:
    case 0x7a:
    case 0xda:
    case 0xfa:
    break;
    
    case 0x80:
    case 0x82:
    case 0x89:
    case 0xc2:
    case 0xe2:
    case 0x04:
    case 0x44:
    case 0x64:
    case 0x14:
    case 0x34:
    case 0x54:
    case 0x74:
    case 0xd4:
    case 0xf4:
    pc++;
    break;
    
    case 0x0c:
    case 0x1c:
    case 0x3c:
    case 0x5c:
    case 0x7c:
    case 0xdc:
    case 0xfc:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    pc += 2;
    break;

    case 0x69:
    ADC(IMMEDIATE());
    pc++;
    break;

    case 0x65:
    ADC(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x75:
    ADC(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0x6d:
    ADC(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x7d:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    ADC(MEM(ABSOLUTEX()));
     pc += 2;
    break;

    case 0x79:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    ADC(MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0x61:
    ADC(MEM(INDIRECTX()));
    pc++;
    break;

    case 0x71:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    ADC(MEM(INDIRECTY()));
    pc++;
    break;

    case 0x29:
    AND(IMMEDIATE());
    pc++;
    break;

    case 0x25:
    AND(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x35:
    AND(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0x2d:
    AND(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x3d:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    AND(MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0x39:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    AND(MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0x21:
    AND(MEM(INDIRECTX()));
    pc++;
    break;

    case 0x31:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    AND(MEM(INDIRECTY()));
    pc++;
    break;

    case 0x0a:
    ASL(a);
    break;

    case 0x06:
    ASL(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x16:
    ASL(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0x0e:
    ASL(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x1e:
    ASL(MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0x90:
    if (!(flags & FC)) BRANCH()
    else pc++;
    break;

    case 0xb0:
    if (flags & FC) BRANCH()
    else pc++;
    break;

    case 0xf0:
    if (flags & FZ) BRANCH()
    else pc++;
    break;

    case 0x24:
    BIT(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x2c:
    BIT(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x30:
    if (flags & FN) BRANCH()
    else pc++;
    break;

    case 0xd0:
    if (!(flags & FZ)) BRANCH()
    else pc++;
    break;

    case 0x10:
    if (!(flags & FN)) BRANCH()
    else pc++;
    break;

    case 0x50:
    if (!(flags & FV)) BRANCH()
    else pc++;
    break;

    case 0x70:
    if (flags & FV) BRANCH()
    else pc++;
    break;

    case 0x18:
    flags &= ~FC;
    break;

    case 0xd8:
    flags &= ~FD;
    break;

    case 0x58:
    flags &= ~FI;
    break;

    case 0xb8:
    flags &= ~FV;
    break;

    case 0xc9:
    CMP(a, IMMEDIATE());
    pc++;
    break;

    case 0xc5:
    CMP(a, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xd5:
    CMP(a, MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0xcd:
    CMP(a, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xdd:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    CMP(a, MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0xd9:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    CMP(a, MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0xc1:
    CMP(a, MEM(INDIRECTX()));
    pc++;
    break;

    case 0xd1:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    CMP(a, MEM(INDIRECTY()));
    pc++;
    break;

    case 0xe0:
    CMP(x, IMMEDIATE());
    pc++;
    break;

    case 0xe4:
    CMP(x, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xec:
    CMP(x, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xc0:
    CMP(y, IMMEDIATE());
    pc++;
    break;

    case 0xc4:
    CMP(y, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xcc:
    CMP(y, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xc6:
    DEC(MEM(ZEROPAGE()));
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0xd6:
    DEC(MEM(ZEROPAGEX()));
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0xce:
    DEC(MEM(ABSOLUTE()));
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0xde:
    DEC(MEM(ABSOLUTEX()));
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0xca:
    x--;
    SETFLAGS(x);
    break;

    case 0x88:
    y--;
    SETFLAGS(y);
    break;

    case 0x49:
    EOR(IMMEDIATE());
    pc++;
    break;

    case 0x45:
    EOR(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x55:
    EOR(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0x4d:
    EOR(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x5d:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    EOR(MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0x59:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    EOR(MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0x41:
    EOR(MEM(INDIRECTX()));
    pc++;
    break;

    case 0x51:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    EOR(MEM(INDIRECTY()));
    pc++;
    break;

    case 0xe6:
    INC(MEM(ZEROPAGE()));
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0xf6:
    INC(MEM(ZEROPAGEX()));
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0xee:
    INC(MEM(ABSOLUTE()));
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0xfe:
    INC(MEM(ABSOLUTEX()));
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0xe8:
    x++;
    SETFLAGS(x);
    break;

    case 0xc8:
    y++;
    SETFLAGS(y);
    break;

    case 0x20:
    PUSH((pc+1) >> 8);
    PUSH((pc+1) & 0xff);
    pc = ABSOLUTE();
    break;

    case 0x4c:
    pc = ABSOLUTE();
    break;

    case 0x6c:
    {
      unsigned short adr = ABSOLUTE();
      pc = (MEM(adr) | (MEM(((adr + 1) & 0xff) | (adr & 0xff00)) << 8));
    }
    break;

    case 0xa9:
    ASSIGNSETFLAGS(a, IMMEDIATE());
    pc++;
    break;

    case 0xa5:
    ASSIGNSETFLAGS(a, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xb5:
    ASSIGNSETFLAGS(a, MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0xad:
    ASSIGNSETFLAGS(a, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xbd:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    ASSIGNSETFLAGS(a, MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0xb9:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    ASSIGNSETFLAGS(a, MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0xa1:
    ASSIGNSETFLAGS(a, MEM(INDIRECTX()));
    pc++;
    break;

    case 0xb1:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    ASSIGNSETFLAGS(a, MEM(INDIRECTY()));
    pc++;
    break;

    case 0xa2:
    ASSIGNSETFLAGS(x, IMMEDIATE());
    pc++;
    break;

    case 0xa6:
    ASSIGNSETFLAGS(x, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xb6:
    ASSIGNSETFLAGS(x, MEM(ZEROPAGEY()));
    pc++;
    break;

    case 0xae:
    ASSIGNSETFLAGS(x, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xbe:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    ASSIGNSETFLAGS(x, MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0xa0:
    ASSIGNSETFLAGS(y, IMMEDIATE());
    pc++;
    break;

    case 0xa4:
    ASSIGNSETFLAGS(y, MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xb4:
    ASSIGNSETFLAGS(y, MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0xac:
    ASSIGNSETFLAGS(y, MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xbc:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    ASSIGNSETFLAGS(y, MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0x4a:
    LSR(a);
    break;

    case 0x46:
    LSR(MEM(ZEROPAGE()));
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x56:
    LSR(MEM(ZEROPAGEX()));
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0x4e:
    LSR(MEM(ABSOLUTE()));
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0x5e:
    LSR(MEM(ABSOLUTEX()));
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0xea:
    break;

    case 0x09:
    ORA(IMMEDIATE());
    pc++;
    break;

    case 0x05:
    ORA(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0x15:
    ORA(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0x0d:
    ORA(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0x1d:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    ORA(MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0x19:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    ORA(MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0x01:
    ORA(MEM(INDIRECTX()));
    pc++;
    break;

    case 0x11:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    ORA(MEM(INDIRECTY()));
    pc++;
    break;

    case 0x48:
    PUSH(a);
    break;

    case 0x08:
    PUSH(flags | 0x30);
    break;

    case 0x68:
    ASSIGNSETFLAGS(a, POP());
    break;

    case 0x28:
    flags = POP();
    break;

    case 0x2a:
    ROL(a);
    break;

    case 0x26:
    ROL(MEM(ZEROPAGE()));
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x36:
    ROL(MEM(ZEROPAGEX()));
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0x2e:
    ROL(MEM(ABSOLUTE()));
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0x3e:
    ROL(MEM(ABSOLUTEX()));
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0x6a:
    ROR(a);
    break;

    case 0x66:
    ROR(MEM(ZEROPAGE()));
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x76:
    ROR(MEM(ZEROPAGEX()));
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0x6e:
    ROR(MEM(ABSOLUTE()));
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0x7e:
    ROR(MEM(ABSOLUTEX()));
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0x40:
    if (sp == 0xff) return 0;
    flags = POP();
    pc = POP();
    pc |= POP() << 8;
    break;

    case 0x60:
    if (sp == 0xff) return 0;
    pc = POP();
    pc |= POP() << 8;
    pc++;
    break;

    case 0xe9:
    case 0xeb:
    SBC(IMMEDIATE());
    pc++;
    break;

    case 0xe5:
    SBC(MEM(ZEROPAGE()));
    pc++;
    break;

    case 0xf5:
    SBC(MEM(ZEROPAGEX()));
    pc++;
    break;

    case 0xed:
    SBC(MEM(ABSOLUTE()));
    pc += 2;
    break;

    case 0xfd:
    cpucycles += EVALPAGECROSSING_ABSOLUTEX();
    SBC(MEM(ABSOLUTEX()));
    pc += 2;
    break;

    case 0xf9:
    cpucycles += EVALPAGECROSSING_ABSOLUTEY();
    SBC(MEM(ABSOLUTEY()));
    pc += 2;
    break;

    case 0xe1:
    SBC(MEM(INDIRECTX()));
    pc++;
    break;

    case 0xf1:
    cpucycles += EVALPAGECROSSING_INDIRECTY();
    SBC(MEM(INDIRECTY()));
    pc++;
    break;

    case 0x38:
    flags |= FC;
    break;

    case 0xf8:
    flags |= FD;
    break;

    case 0x78:
    flags |= FI;
    break;

    case 0x85:
    MEM(ZEROPAGE()) = a;
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x95:
    MEM(ZEROPAGEX()) = a;
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0x8d:
    MEM(ABSOLUTE()) = a;
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0x9d:
    MEM(ABSOLUTEX()) = a;
    WRITE(ABSOLUTEX());
    pc += 2;
    break;

    case 0x99:
    MEM(ABSOLUTEY()) = a;
    WRITE(ABSOLUTEY());
    pc += 2;
    break;

    case 0x81:
    MEM(INDIRECTX()) = a;
    WRITE(INDIRECTX());
    pc++;
    break;

    case 0x91:
    MEM(INDIRECTY()) = a;
    WRITE(INDIRECTY());
    pc++;
    break;

    case 0x86:
    MEM(ZEROPAGE()) = x;
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x96:
    MEM(ZEROPAGEY()) = x;
    WRITE(ZEROPAGEY());
    pc++;
    break;

    case 0x8e:
    MEM(ABSOLUTE()) = x;
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0x84:
    MEM(ZEROPAGE()) = y;
    WRITE(ZEROPAGE());
    pc++;
    break;

    case 0x94:
    MEM(ZEROPAGEX()) = y;
    WRITE(ZEROPAGEX());
    pc++;
    break;

    case 0x8c:
    MEM(ABSOLUTE()) = y;
    WRITE(ABSOLUTE());
    pc += 2;
    break;

    case 0xaa:
    ASSIGNSETFLAGS(x, a);
    break;

    case 0xba:
    ASSIGNSETFLAGS(x, sp);
    break;

    case 0x8a:
    ASSIGNSETFLAGS(a, x);
    break;

    case 0x9a:
    sp = x;
    break;

    case 0x98:
    ASSIGNSETFLAGS(a, y);
    break;

    case 0xa8:
    ASSIGNSETFLAGS(y, a);
    break;

    case 0x00:
    return 0;

    case 0x02:
    printf("Error: CPU halt at %04X\n", pc-1);
    exit(1);
    break;
          
    default:
    printf("Error: Unknown opcode $%02X at $%04X\n", op, pc-1);
    exit(1);
    break;
  }
  return 1;
}

void setpc(unsigned short newpc)
{
  pc = newpc;
}

