
namespace Snapshot {
    void save_z80(const char *path, Z80 *cpu, ULA *ula, IO *io);
    Hardware load_z80(const char *path, Z80 *cpu, ULA *ula, IO *io);
}

