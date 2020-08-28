#pragma once

#include <memory>
#include <string>
#include "shared.h"
#include "IHostSystem.h"
#include "GameBoyCart.h"
#include "GameBoyCpu.h"
#include "GameBoyPpu.h"
#include "GameBoyApu.h"

enum class GameBoyModel
{
    Auto,
    GameBoy,
    GameBoyColor,
    SuperGameBoy,
};

enum IrqFlag : u8
{
    // highest to lowest priority
    VBlank = 1 << 0,
    LcdStat = 1 << 1,
    Timer = 1 << 2,
    Serial = 1 << 3,
    JoyPad = 1 << 4
};

struct GameBoyState
{
    u64 cycleCount;

    // Bit 4 - Joypad Interrupt Requested
    // Bit 3 - Serial Interrupt Requested
    // Bit 2 - Timer Interrupt Requested
    // Bit 1 - LCD STAT Interrupt Requested
    // Bit 0 - Vertical Blank Interrupt Requested
    u8 interruptFlags;

    // Bit 4 - Joypad Interrupt Enable
    // Bit 3 - Serial Interrupt Enable
    // Bit 2 - Timer Interrupt Enable
    // Bit 1 - LCD STAT Interrupt Enable
    // Bit 0 - Vertical Blank Interrupt Enable
    u8 interruptEnable;

    // 8 bits of data to be read/written over serial port
    u8 serialTransfer;

    // Bit 7 - Transfer Start Flag
    //  0: No transfer
    //  1: Start transfer
    // Bit 0 - Shift Clock
    //  0: External Clock (500KHz Max.)
    //  1: Internal Clock (8192Hz)
    u8 serialControl;

    // Incremented at 16.384KHz. Writing anything sets it to 0.
    u16 divider;

    // Incremented according to frequency in TAC (FF07)
    // Generates an interrupt on overflow
    u8 timerCounter;

    // When the TIMA overflows, this data will be loaded.
    u8 timerModulo;

    // Bit 2: Timer Start/Stop
    // Bit 1-0: Input Clock Select
    //  00: 4.096 KHz
    //  01: 262.144 KHz
    //  10: 65.536 KHz
    //  11: 16.384 KHz
    u8 timerControl;

    // Dividier for input clock (controlled by TimerControl reg)
    u16 timerDivider;

    // track if counter was just loaded with Modulo during this cycle
    bool timerResetting;

    // track if the counter is about to be reset back to Modulo
    bool timerResetPending;

    // Source address upper byte (XX00h) for OAM DMA transfer
    u8 oamDmaSrcAddr;

    // waiting for next cycle to start OAM DMA
    bool pendingOamDmaStart;

    // counts down from 161 to 0
    u8 oamDmaCounter;

    // stores the buffered value from the previous cycle's read
    u8 oamDmaBuffer;

    // select joypad line to read
    u8 joyPadInputSelect;

    // Enable CGB registers, etc.
    bool isCgb;

    // Flag that indicates if CGB is put into high-speed mode
    bool cgbHighSpeed;

    // Program is requesting to toggle between high/low-speed
    bool cgbPrepareSpeedSwitch;

    // Work RAM bank select (CGB)
    u8 cgbRamBank;

    // Source address for DMA transfer (CGB)
    u16 cgbDmaSrcAddr;

    // Destination address for DMA transfer (CGB)
    u16 cgbDmaDestAddr;

    // Number of bytes left to transfer (CGB), 7-bits
    u16 cgbDmaLength;

    // Flag that indicates a DMA transfer was completed
    bool cgbDmaComplete;

    // Indicates that the DMA transfer is operating in HMDA mode
    bool cgbHdmaMode;
};

class GameBoy
{
private:
    static constexpr u32 WorkRamSize = 0x2000; // 8 KB
    static constexpr u32 WorkRamSizeCgb = 0x8000; // 32 KB
    static constexpr u32 VideoRamSize = 0x2000; // 8 KB
    static constexpr u32 VideoRamSizeCgb = 0x4000; // 16 KB
	static constexpr u32 OamRamSize = 0xA0;
	static constexpr u32 HighRamSize = 0x7F;

    IHostSystem *_host;

    GameBoyModel _model;
    GameBoyState _state = {};

    std::unique_ptr<GameBoyCart> _cart;
    std::unique_ptr<GameBoyCpu> _cpu;
    std::unique_ptr<GameBoyPpu> _ppu;
    std::unique_ptr<GameBoyApu> _apu;

    // internal RAM
    u8 *_workRam = nullptr;
    u8 *_videoRam = nullptr;
    u8 *_oamRam = nullptr;
    u8 *_highRam = nullptr;

    // depends on model selected
    u32 _workRamSize = 0;
    u32 _videoRamSize = 0;

    // memory mapping (64 KB address space is divided into 256 blocks that are 256 bytes long)
    // each block maps to ROM or RAM both internally or cartridge, writes to I/O registers are intercepted earlier
    u8 *_readMap[0x100] = {};
    u8 *_writeMap[0x100] = {};
    u8 _readableRegMap[0x100] = {};
    u8 _writeableRegMap[0x100] = {};
public:
    GameBoy(GameBoyModel type, const char *romFile, IHostSystem *host);
    ~GameBoy();

    u64 GetCycleCount() { return _state.cycleCount; }
    u32 *GetPixelBuffer() { return _ppu->GetPixelBuffer(); }
    GameBoyModel GetModel() { return _model; }
    inline bool IsCgb() { return _state.isCgb; }

    void ExecuteTwoCycles();
    void Reset();
    void RunCycles(u32 cycles);
    void RunOneFrame();
    
    void SwitchSpeed();
    bool IsSwitchingSpeed() { return _state.cgbPrepareSpeedSwitch; }
    bool IsHighSpeed() { return _state.cgbHighSpeed; }

    u8 Read(u16 addr);
    u8 ReadRegister(u16 addr);
    void Write(u16 addr, u8 val);
    void WriteRegister(u16 addr, u8 val);

    // maps "src" data into address ranges from start to end, if readOnly then only map into "read" mapping
    // NOTE: start/end must align to 256 byte blocks
    void MapMemory(u8 *src, u16 start, u16 end, bool readOnly);
    void UnmapMemory(u16 start, u16 end);
    void MapRegisters(u16 start, u16 end, bool canRead, bool canWrite);
    void UnmapRegisters(u16 start, u16 end);

    u8 GetJoyPadState();

    // interrupts
    u8 GetPendingInterrupt();
    void ResetInterruptFlags(u8 val);
    void SetInterruptFlags(u8 val);

    // timer
    void ResetTimerCounter();
    inline void ExecuteTimer();

    // dma
    inline void ExecuteOamDma();
    inline void ExecuteCgbDma();
    void ExecuteCgbHdma();
};