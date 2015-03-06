#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "api/include/pd_readwrite.h"
#include "api/src/remote/pd_readwrite_private.h"
#include "core/core.h"
#include "core/plugin_handler.h"
#include "core/process.h"
#include "core/time.h"
#include "core/math.h"
#include "session/session.h"
#include "session/session_private.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CPUState
{
    uint16_t pc;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static Session* s_session;
static ProcessHandle s_viceHandle;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void test_c64_vice_init(void**)
{
    int count = 0;

    assert_true(PluginHandler_addPlugin(OBJECT_DIR, "c64_vice_plugin"));
    assert_non_null(PluginHandler_getPlugins(&count)[0]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void test_c64_vice_fail_connect(void**)
{
    int count = 0;

    PluginData* pluginData;

    pluginData = PluginHandler_getPlugins(&count)[0];

    s_session = Session_createLocal((PDBackendPlugin*)pluginData->plugin, 0);

    Session_update(s_session);

    // We haven't setup vice at this point so no connect

    assert_int_equal(s_session->state, PDDebugState_noTarget);

    Session_destroy(s_session);

    s_session = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void updateRegisters(CPUState* cpuState, PDReader* reader)
{
    PDReaderIterator it;

    if (PDRead_findArray(reader, &it, "registers", 0) == PDReadStatus_notFound)
        return;

    while (PDRead_getNextEntry(reader, &it))
    {
        const char* name = "";
        uint16_t regValue;

        PDRead_findString(reader, &name, "name", it);
        PDRead_findU16(reader, &regValue, "register", it);

        if (!strcmp(name, "pc"))
            cpuState->pc = (uint16_t)regValue;
        else if (!strcmp(name, "sp"))
            cpuState->sp = (uint8_t)regValue;
        else if (!strcmp(name, "a"))
            cpuState->a = (uint8_t)regValue;
        else if (!strcmp(name, "x"))
            cpuState->x = (uint8_t)regValue;
        else if (!strcmp(name, "y"))
            cpuState->y = (uint8_t)regValue;

    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void handleEvents(CPUState* cpuState, Session* session)
{
    PDReader* reader = session->reader;

    PDBinaryReader_initStream(reader, PDBinaryWriter_getData(session->currentWriter), PDBinaryWriter_getSize(session->currentWriter));

    uint32_t event;
    bool foundRegisters = false;

    while ((event = PDRead_getEvent(reader)) != 0)
    {
        switch (event)
        {
            case PDEventType_setRegisters:
            {
                updateRegisters(cpuState, reader);
                foundRegisters = true;
                break;
            }
        }
    }

    (void)cpuState;

    assert_true(foundRegisters);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void printCPUState(CPUState* state)
{
    printf("pc %04x - a %02x - x - %02x - y %02x - sp %02x\n", state->pc, state->a, state->x, state->y, state->sp);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void test_c64_vice_connect(void**)
{
    int count = 0;

    PluginData* pluginData;

    pluginData = PluginHandler_getPlugins(&count)[0];

    // Lanuch C64 VICE
    // TODO: Fix hardcoded path

#ifdef PRODBG_MAC
    const char* viceLaunchPath = "../../vice/x64.app/Contents/MacOS/x64";
#elif PRODBG_WIN
    const char* viceLaunchPath = "..\\..\\vice\\x64.exe";
#else
    // Not supported on Linux yet
    const char* viceLaunchPath = 0;
#endif
    assert_non_null(viceLaunchPath);

    const char* argv[] = { viceLaunchPath, "-remotemonitor", "/Users/danielcollin/code/temp/test.prg", 0};

    s_viceHandle = Process_spawn(viceLaunchPath, argv);

    assert_non_null(s_viceHandle);

    // Wait 3 sec for VICE to launch

    Time_sleepMs(4000);

    // TODO: Non hard-coded path

    s_session = Session_createLocal((PDBackendPlugin*)pluginData->plugin, 0);

    Session_update(s_session);

    // We haven't setup vice at this point so no connect

    assert_int_not_equal(s_session->state, PDDebugState_noTarget);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void test_c64_vice_get_registers(void**)
{
    CPUState state;

    PDWriter* writer = s_session->currentWriter;

    PDWrite_eventBegin(writer, PDEventType_getRegisters);
    PDWrite_eventEnd(writer);
    PDBinaryWriter_finalize(writer);

    Session_update(s_session);
    handleEvents(&state, s_session);

    assert_true(state.pc >= 0x80e && state.pc <= 0x81a);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void test_c64_vice_step_cpu(void**)
{
    CPUState state;

    Session_action(s_session, PDAction_step);
    handleEvents(&state, s_session);

    assert_true(state.pc >= 0x80e && state.pc <= 0x81a);
    assert_true(state.a == 0x22);
    assert_true(state.x == 0x32);
    assert_true(state.y == 0x42);

    Session_action(s_session, PDAction_step);
    handleEvents(&state, s_session);

    assert_true(state.pc >= 0x80e && state.pc <= 0x81a);

    Session_action(s_session, PDAction_step);
    handleEvents(&state, s_session);

    assert_true(state.pc >= 0x80e && state.pc <= 0x81a);

    // Get registers after some stepping

    PDWriter* writer = s_session->currentWriter;

    PDWrite_eventBegin(writer, PDEventType_getRegisters);
    PDWrite_eventEnd(writer);
    PDBinaryWriter_finalize(writer);

    Session_update(s_session);
    handleEvents(&state, s_session);

    assert_true(state.pc >= 0x80e && state.pc <= 0x81a);

    writer = s_session->currentWriter;

    PDWrite_eventBegin(writer, PDEventType_getRegisters);
    PDWrite_eventEnd(writer);
    PDBinaryWriter_finalize(writer);

    Session_update(s_session);
    handleEvents(&state, s_session);

    assert_true(state.pc >= 0x80e && state.pc <= 0x81a);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Assembly
{
    uint16_t address;
    const char* text;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void test_c64_vice_get_disassembly(void**)
{
    static Assembly assembly[] =
    {
        { 0x080e, "A9 22       LDA #$22" },
        { 0x0810, "A2 32       LDX #$32" },
        { 0x0812, "A0 42       LDY #$42" },
        { 0x0814, "EE 20 D0    INC $D020" },
        { 0x0817, "EE 21 D0    INC $D021" },
        { 0x081a, "4C 0E 08    JMP $080E" },
        { 0, 0 },
    };

    PDWriter* writer = s_session->currentWriter;

    PDWrite_eventBegin(writer, PDEventType_getDisassembly);
    PDWrite_u64(writer, "address_start", 0x80e);
    PDWrite_u32(writer, "instruction_count", (uint32_t)4);
    PDWrite_eventEnd(writer);
    PDBinaryWriter_finalize(writer);

    Session_update(s_session);

    PDReader* reader = s_session->reader;

    PDBinaryReader_initStream(reader, PDBinaryWriter_getData(s_session->currentWriter), PDBinaryWriter_getSize(s_session->currentWriter));

    uint32_t event;

    while ((event = PDRead_getEvent(reader)) != 0)
    {
        if (event != PDEventType_setDisassembly)
            continue;

        PDReaderIterator it;

        assert_false(PDRead_findArray(reader, &it, "disassembly", 0) == PDReadStatus_notFound);

        int i = 0;

        while (PDRead_getNextEntry(reader, &it))
        {
            uint64_t address;
            const char* text;

            PDRead_findU64(reader, &address, "address", it);
            PDRead_findString(reader, &text, "line", it);

            assert_non_null(assembly[i].text);
            assert_int_equal((int)assembly[i].address, (int)address);
            assert_string_equal(assembly[i].text, text);

            i++;
        }

        return;

    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void test_c64_vice_get_memory(void**)
{
    const uint8_t read_memory[] = { 0xa9, 0x22, 0xa2, 0x32, 0xa0, 0x42, 0xee, 0x20, 0xd0, 0xee, 0x21, 0xd0, 0x4c, 0x0e, 0x08 };

    PDWriter* writer = s_session->currentWriter;

    PDWrite_eventBegin(writer, PDEventType_getMemory);
    PDWrite_u64(writer, "address_start", 0x080e);
    PDWrite_u64(writer, "size", (uint32_t)15);
    PDWrite_eventEnd(writer);
    PDBinaryWriter_finalize(writer);

    Session_update(s_session);

    PDReader* reader = s_session->reader;

    PDBinaryReader_initStream(reader, PDBinaryWriter_getData(s_session->currentWriter), PDBinaryWriter_getSize(s_session->currentWriter));

    uint32_t event;

    while ((event = PDRead_getEvent(reader)) != 0)
    {
        uint8_t* data;
        uint64_t dataSize;
        uint64_t address;

        if (event != PDEventType_setMemory)
            continue;

        assert_true(PDRead_findU64(reader, &address, "address", 0) & PDReadStatus_ok);
        assert_true((PDRead_findData(reader, (void**)&data, &dataSize, "data", 0) & PDReadStatus_typeMask) == PDReadType_data);

        assert_true(address == 0x080e);
        assert_true(dataSize >= 15);

        assert_memory_equal(data, read_memory, sizeof_array(read_memory));

        return;
    }

    // no memory found

    fail();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_c64_vice_init),
        unit_test(test_c64_vice_fail_connect),
        unit_test(test_c64_vice_connect),
        unit_test(test_c64_vice_get_registers),
        unit_test(test_c64_vice_step_cpu),
        unit_test(test_c64_vice_get_disassembly),
        unit_test(test_c64_vice_get_memory),
    };

    int test = run_tests(tests);

    if (s_viceHandle)
        Process_kill(s_viceHandle);

    return test;
}
