#pragma once

namespace prodbg
{

typedef void* Handle;

Handle SharedObject_open(const char* filename);
void SharedObject_close(Handle handle);
void* SharedObject_getSym(Handle handle, const char* name);

}

