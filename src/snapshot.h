namespace Snapshot {
    void save_z80(const char *path, Z80_State &cpu, ULA *ula, IO *io);
    Hardware load_z80(const char *path, Z80_State &cpu, ULA *ula, IO *io);
}

