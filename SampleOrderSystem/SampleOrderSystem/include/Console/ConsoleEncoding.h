#pragma once

// Configures the Windows console for UTF-8 input/output. Source files are
// compiled with /utf-8, so string literals (Korean menu text, etc.) are
// encoded as UTF-8 in the binary -- but the console still renders them using
// the OS's default codepage (e.g. CP949 on Korean Windows) unless told
// otherwise, which is what produced garbled text when running from Visual
// Studio. No-op on non-Windows platforms.
namespace ConsoleEncoding {

void configureUtf8();

}  // namespace ConsoleEncoding
