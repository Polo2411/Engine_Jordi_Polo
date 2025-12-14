#include "Globals.h"
#include <vector>
#include <string>

static std::vector<std::string> g_LogLines;  // <- almacena las líneas de LOG

void log(const char file[], int line, const char* format, ...)
{
    static char tmp_string[4096];
    static char tmp_string2[4096];
    static va_list ap;

    // Construct the string from variable arguments
    va_start(ap, format);
    vsprintf_s(tmp_string, 4095, format, ap);
    va_end(ap);

    // Mensaje con fichero/linea para el OutputDebug
    sprintf_s(tmp_string2, 4095, "\n%s(%d) : %s", file, line, tmp_string);
    OutputDebugStringA(tmp_string2);

    // 👇 Guardamos SOLO el mensaje formateado en nuestro buffer de consola
    g_LogLines.emplace_back(tmp_string);
}

// 👇 Devuelve el vector de líneas para que ImGui lo pinte
const std::vector<std::string>& GetLogLines()
{
    return g_LogLines;
}
