
namespace Snapshot {
    void save_z80(const char *path, Z80 *cpu, Memory *memory, IO *io);
    Hardware load_z80(const char *path, Z80 *cpu, Memory *memory, IO *io);
}

