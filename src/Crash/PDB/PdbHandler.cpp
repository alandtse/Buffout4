// SPDX-License-Identifier: CC-BY-SA-4.0
// Code from StackOverflow

#pragma once
#include "PdbHandler.h"
#include <DbgHelp.h>
#include <atlcomcli.h>
#include <codecvt>  // For string conversions
#include <comdef.h>
#include <regex>
#include <unordered_set>

namespace Crash
{
	namespace PDB
	{
		std::atomic<bool> symcacheChecked = false;
		std::atomic<bool> symcacheValid = false;
		//https://stackoverflow.com/questions/6284524/bstr-to-stdstring-stdwstring-and-vice-versa
		std::string ConvertWCSToMBS(const wchar_t* pstr, long wslen)
		{
			int len = ::WideCharToMultiByte(CP_ACP, 0, pstr, wslen, NULL, 0, NULL, NULL);

			std::string dblstr(len, '\0');
			len = ::WideCharToMultiByte(CP_ACP, 0 /* no flags */,
				pstr, wslen /* not necessary NULL-terminated */,
				&dblstr[0], len,
				NULL, NULL /* no default char */);

			return dblstr;
		}

		std::string ConvertBSTRToMBS(BSTR bstr)
		{
			int wslen = ::SysStringLen(bstr);
			return ConvertWCSToMBS((wchar_t*)bstr, wslen);
		}

		BSTR ConvertMBSToBSTR(const std::string& str)
		{
			int wslen = ::MultiByteToWideChar(CP_ACP, 0 /* no flags */,
				str.data(), static_cast<int>(str.length()),
				NULL, 0);

			BSTR wsdata = ::SysAllocStringLen(NULL, wslen);
			::MultiByteToWideChar(CP_ACP, 0 /* no flags */,
				str.data(), static_cast<int>(str.length()),
				wsdata, wslen);
			return wsdata;
		}

		std::wstring utf8_to_utf16(const std::string& utf8)
		{
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			return converter.from_bytes(utf8);
		}

		std::string utf16_to_utf8(const std::wstring& utf16)
		{
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			return converter.to_bytes(utf16);
		}

		/*[[nodiscard]] static std::string trim(const std::string& str)
		{
			const auto start = str.find_first_not_of(" \t\n\r");
			const auto end = str.find_last_not_of(" \t\n\r");
			return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
		}*/

		[[nodiscard]] static std::wstring trim(const std::wstring& wstr)
		{
			auto start = wstr.begin();
			while (start != wstr.end() && std::iswspace(*start)) {
				++start;
			}

			auto end = wstr.end();
			do {
				--end;
			} while (end != start && std::iswspace(*end));

			return std::wstring(start, end + 1);
		}

		[[nodiscard]] static std::string demangle(const std::wstring& mangled)
		{
			// Set of ignored demangled types
			static const std::unordered_set<std::wstring> ignoredTypes = {
				L"float",
				L"char",
				L"signed char",
				L"unsigned char",
				L"void",
				L"short",
				L"unsigned short",
				L"double",
				L"unsigned char volatile",
			};

			const auto demangle_single = [](const wchar_t* a_in, wchar_t* a_out, std::uint32_t a_size) {
				static std::mutex m;
				std::lock_guard l{ m };
				return UnDecorateSymbolNameW(
					a_in,
					a_out,
					a_size,
					(UNDNAME_NO_MS_KEYWORDS) |
						(UNDNAME_NO_FUNCTION_RETURNS) |
						(UNDNAME_NO_ALLOCATION_MODEL) |
						(UNDNAME_NO_ALLOCATION_LANGUAGE) |
						(UNDNAME_NO_THISTYPE) |
						(UNDNAME_NO_ACCESS_SPECIFIERS) |
						(UNDNAME_NO_THROW_SIGNATURES) |
						(UNDNAME_NO_RETURN_UDT_MODEL) |
						(UNDNAME_NAME_ONLY) |
						(UNDNAME_NO_ARGUMENTS) |
						static_cast<std::uint32_t>(0x8000));
			};

			// Buffer to store demangled result
			std::array<wchar_t, 0x1000> buf{ L'\0' };
			std::wistringstream wiss(mangled);
			std::wostringstream woss;

			std::wstring word;
			bool hasDemangled = false;

			while (wiss >> word) {
				// Log the word before demangling
				logger::info("Demangling word: {}", utf16_to_utf8(word));

				// Attempt to demangle each word
				const auto len = demangle_single(word.c_str(), buf.data(), static_cast<std::uint32_t>(buf.size()));

				// Ensure null-termination
				buf[len] = L'\0';

				std::wstring demangledWord{ buf.data() };

				// Trim the demangled word
				demangledWord = trim(demangledWord);

				// Log the demangled word
				logger::info("Demangled result: {}", utf16_to_utf8(demangledWord));

				// Check if the demangled result is different and not in the ignored set
				if (len != 0 && demangledWord != L"<unknown>" && demangledWord != L"UNKNOWN") {
					if (ignoredTypes.find(demangledWord) != ignoredTypes.end() || demangledWord.starts_with(L"?? ::")) {
						woss << word << L" ";  // Keep the original mangled word
					} else {
						woss << demangledWord << L" ";
						hasDemangled = true;
					}
				} else {
					woss << word << L" ";  // If demangling failed, keep the original mangled word
				}
			}

			// Prepare the final result
			std::wstring result = woss.str();
			std::wstring trimmedResult = trim(result);

			// Extract the potential replacement word from the mangled string if we have a valid demangled result
			std::wstring replacementWord;
			if (hasDemangled && mangled.starts_with(L'?')) {
				size_t start = 1;  // Skip the initial '?'
				size_t atPos = mangled.find(L'@', start);
				if (atPos != std::wstring::npos) {
					replacementWord = mangled.substr(start, atPos - start);
					logger::info("Found potential replacement word: {}", utf16_to_utf8(replacementWord));
				}
			}

			// Check for potential truncation only if the demangled result is used
			if (hasDemangled && trimmedResult != mangled) {
				std::wregex endsWithPattern(L"::[\\w]+$");
				std::wsmatch match;

				if (std::regex_search(trimmedResult, match, endsWithPattern)) {
					// Extract the word after '::'
					std::wstring truncatedPart = match.str(0).substr(2);  // Remove '::'
					logger::info("Found potential truncatedPart: {}", utf16_to_utf8(truncatedPart));

					// Compare the truncated part with the replacement word
					if (!replacementWord.empty() && !truncatedPart.empty() && replacementWord.ends_with(truncatedPart)) {
						// Replace the truncated part with the replacement word
						trimmedResult.replace(match.position(0), match.length(0), L"::" + replacementWord);
					}
				}
			}

			// If the demangled string is different from the original, return both
			if (hasDemangled && trimmedResult != mangled) {
				return fmt::format("{} (mangled: {})", utf16_to_utf8(trimmedResult), utf16_to_utf8(mangled));
			}

			// Otherwise, return the original mangled string
			return utf16_to_utf8(mangled);
		}

		std::string processSymbol(IDiaSymbol* a_symbol, IDiaSession* a_session, const DWORD& a_rva, std::string_view& a_name, uintptr_t& a_offset, std::string& a_result)
		{
			BSTR name;
			a_symbol->get_name(&name);

			// Demangle the symbol name
			std::string demangledName = demangle(name);

			DWORD rva;
			if (a_rva == 0)
				a_symbol->get_relativeVirtualAddress(&rva);  // find rva if not provided
			else
				rva = a_rva;

			ULONGLONG length = 0;
			if (a_symbol->get_length(&length) == S_OK) {
				IDiaEnumLineNumbers* lineNums[100];
				if (a_session->findLinesByRVA(rva, static_cast<DWORD>(length), lineNums) == S_OK) {
					auto& lineNumsPtr = lineNums[0];
					CComPtr<IDiaLineNumber> line;
					IDiaLineNumber* lineNum;
					ULONG fetched = 0;
					bool found_source = false;
					bool found_line = false;

					for (uint8_t i = 0; i < 5; ++i) {
						if (lineNumsPtr->Next(i, &lineNum, &fetched) == S_OK && fetched == 1) {
							found_source = false;
							found_line = false;
							DWORD sline;
							IDiaSourceFile* srcFile;
							BSTR fileName = nullptr;
							std::string convertedFileName;

							if (lineNum->get_sourceFile(&srcFile) == S_OK) {
								srcFile->get_fileName(&fileName);
								convertedFileName = ConvertBSTRToMBS(fileName);
								found_source = true;
							}

							if (lineNum->get_lineNumber(&sline) == S_OK)
								found_line = true;

							if (found_source && found_line)
								a_result += fmt::format(" {}:{} {}", convertedFileName, +sline ? (uint64_t)sline : 0, demangledName);
							else if (found_source)
								a_result += fmt::format(" {} {}", convertedFileName, demangledName);
							else if (found_line)
								a_result += fmt::format(" unk_:{} {}", +sline ? (uint64_t)sline : 0, demangledName);
						}
					}

					if (!found_source && !found_line) {
						auto sRva = fmt::format("{:X}", rva);
						if (demangledName.ends_with(sRva))
							sRva = "";
						else
							sRva = "_" + sRva;

						a_result += fmt::format(" {}{}", demangledName, sRva);
					}
				}
			}

			if (a_result.empty())
				logger::info("No symbol found for {}+{:07X}"sv, a_name, a_offset);
			else
				logger::info("Symbol returning: {}", a_result);

			return a_result;
		}

		std::string print_hr_failure(HRESULT a_hr)
		{
			auto errMsg = "";
			switch ((unsigned int)a_hr) {
			case 0x806D0005:  // E_PDB_NOT_FOUND
				errMsg = "Unable to locate PDB";
				break;
			case 0x806D0012:  // E_PDB_FORMAT
			case 0x806D0014:  // E_PDB_NO_DEBUG_INFO
				errMsg = "Invalid or obsolete file format";
				break;
			default:
				_com_error err(a_hr);
				errMsg = CT2A(err.ErrorMessage());
			}
			return errMsg;
		}

		//https://stackoverflow.com/questions/68412597/determining-source-code-filename-and-line-for-function-using-visual-studio-pdb
		std::string pdb_details(std::string_view a_name, uintptr_t a_offset)
		{
			static std::mutex sync;
			std::lock_guard l{ sync };
			std::string result;

			std::filesystem::path dllPath{ a_name };
			std::string dll_path = a_name.data();
			if (!dllPath.has_parent_path()) {
				dll_path = Crash::PDB::sPluginPath.data() + dllPath.filename().string();
			}

			auto rva = static_cast<DWORD>(a_offset);
			CComPtr<IDiaDataSource> pSource;
			hr = S_OK;

			// Initialize COM
			if (FAILED(hr = CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
				auto error = print_hr_failure(hr);
				logger::info("Failed to initialize COM library for dll {}+{:07X}\t{}", a_name, a_offset, error);
				return result;
			}

			// Attempt to load msdia140.dll
			auto* msdia_dll = L"Data/F4SE/Plugins/msdia140.dll";
			hr = NoRegCoCreate(msdia_dll, CLSID_DiaSource, __uuidof(IDiaDataSource), (void**)&pSource);
			if (FAILED(hr)) {
				auto error = print_hr_failure(hr);
				logger::info("Failed to manually load msdia140.dll for dll {}+{:07X}\t{}", a_name, a_offset, error);

				// Try registered copy
				if (FAILED(hr = CoCreateInstance(CLSID_DiaSource, NULL, CLSCTX_INPROC_SERVER, __uuidof(IDiaDataSource), (void**)&pSource))) {
					error = print_hr_failure(hr);
					logger::info("Failed to load registered msdia140.dll for dll {}+{:07X}\t{}", a_name, a_offset, error);
					CoUninitialize();
					return result;
				}
			}

			wchar_t wszFilename[_MAX_PATH];
			wchar_t wszPath[_MAX_PATH];

			// Convert UTF-8 dll_path to UTF-16
			std::wstring dll_path_w = utf8_to_utf16(dll_path);
			wcsncpy(wszFilename, dll_path_w.c_str(), sizeof(wszFilename) / sizeof(wchar_t));

			auto symcache = *Settings::Symcache;

			if (!symcacheChecked) {
				if (!symcache.empty() && std::filesystem::exists(symcache) && std::filesystem::is_directory(symcache)) {
					logger::info("Symcache found at {}", symcache);
					symcacheValid = true;
				} else {
					logger::info("Symcache not found at {}", symcache.empty() ? "not defined" : symcache);
				}
				symcacheChecked = true;
			}

			std::vector<std::string> searchPaths = { Crash::PDB::sPluginPath.data() };

			if (symcacheValid) {
				searchPaths.push_back(fmt::format(fmt::runtime("cache*{}"s), symcache.c_str()));
			}

			bool foundPDB = false;
			for (const auto& path : searchPaths) {
				// Convert UTF-8 path to UTF-16
				std::wstring path_w = utf8_to_utf16(path);
				wcsncpy(wszPath, path_w.c_str(), sizeof(wszPath) / sizeof(wchar_t));

				logger::info("Attempting to find pdb for {}+{:07X} with path {}", a_name, a_offset, path);
				hr = pSource->loadDataForExe(wszFilename, wszPath, NULL);
				if (FAILED(hr)) {
					auto error = print_hr_failure(hr);
					logger::info("Failed to open pdb for dll {}+{:07X}\t{}", a_name, a_offset, error);
					continue;
				}
				foundPDB = true;
				break;
			}

			if (!foundPDB) {
				CoUninitialize();
				return result;
			}

			logger::info("Successfully opened pdb for dll {}+{:07X}", a_name, a_offset);

			// Rest of the PDB processing logic remains the same
			// No other changes needed for UTF-8 handling
			CComPtr<IDiaSession> pSession;
			CComPtr<IDiaSymbol> globalSymbol;
			CComPtr<IDiaEnumTables> enumTables;
			CComPtr<IDiaEnumSymbolsByAddr> enumSymbolsByAddr;

			if (FAILED(hr = pSource->openSession(&pSession))) {
				auto error = print_hr_failure(hr);
				logger::info("Failed to open IDiaSession for pdb for dll {}+{:07X}\t{}", a_name, a_offset, error);
				CoUninitialize();
				return result;
			}

			if (FAILED(hr = pSession->get_globalScope(&globalSymbol))) {
				auto error = print_hr_failure(hr);
				logger::info("Failed to getGlobalScope for pdb for dll {}+{:07X}\t{}", a_name, a_offset, error);
				CoUninitialize();
				return result;
			}

			if (FAILED(hr = pSession->getEnumTables(&enumTables))) {
				auto error = print_hr_failure(hr);
				logger::info("Failed to getEnumTables for pdb for dll {}+{:07X}\t{}", a_name, a_offset, error);
				CoUninitialize();
				return result;
			}

			if (FAILED(hr = pSession->getSymbolsByAddr(&enumSymbolsByAddr))) {
				auto error = print_hr_failure(hr);
				logger::info("Failed to getSymbolsByAddr for pdb for dll {}+{:07X}\t{}", a_name, a_offset, error);
				CoUninitialize();
				return result;
			}

			CComPtr<IDiaSymbol> publicSymbol;
			if (pSession->findSymbolByRVA(rva, SymTagEnum::SymTagPublicSymbol, &publicSymbol) == S_OK) {
				auto publicResult = processSymbol(publicSymbol, pSession, rva, a_name, a_offset, result);

				// Log the public result (already demangled in processSymbol)
				logger::info("Public symbol found for {}+{:07X}: {}", a_name, a_offset, publicResult);

				DWORD privateRva;
				CComPtr<IDiaSymbol> privateSymbol;
				if (publicSymbol->get_targetRelativeVirtualAddress(&privateRva) == S_OK &&
					pSession->findSymbolByRVA(privateRva, SymTagEnum::SymTagFunction, &privateSymbol) == S_OK) {
					auto privateResult = processSymbol(privateSymbol, pSession, privateRva, a_name, a_offset, result);

					// Log the private result (already demangled in processSymbol)
					logger::info("Private symbol found for {}+{:07X}: {}", a_name, a_offset, privateResult);

					// Combine results
					if (!privateResult.empty() && !publicResult.empty()) {
						result = fmt::format("{}\t{}", privateResult, publicResult);
					} else if (!privateResult.empty()) {
						result = privateResult;
					} else {
						result = publicResult;
					}
				} else {
					result = publicResult;
				}
			} else {
				logger::info("No public symbol found for {}+{:07X}", a_name, a_offset);
			}

			CoUninitialize();
			return result;
		}

		// dump all symbols in Plugin directory or fakepdb for exe
		// this was the early POC test and written first in this module
		void dump_symbols(bool exe)
		{
			CoInitialize(nullptr);
			int retflag;
			if (exe) {
				const auto string_path = "./Fallout4VR.exe";
				std::filesystem::path file_path{ string_path };
				dumpFileSymbols(file_path, retflag);
			} else {
				for (const auto& elem : std::filesystem::directory_iterator(Crash::PDB::sPluginPath)) {
					if (const auto filename =
							elem.path().has_filename() ?
								std::make_optional(elem.path().filename().string()) :
								std::nullopt;
						filename.value().ends_with("dll")) {
						dumpFileSymbols(elem.path(), retflag);
						if (retflag == 3)
							continue;
					}
				}
			}
		}
		void dumpFileSymbols(const std::filesystem::path& path, int& retflag)
		{
			retflag = 1;
			const auto filename = std::make_optional(path.filename().string());
			logger::info("Found dll {}", *filename);
			auto dll_path = path.string();
			auto search_path = Crash::PDB::sPluginPath.data();
			search_path;
			CComPtr<IDiaDataSource> source;
			hr = CoCreateInstance(CLSID_DiaSource,
				NULL,
				CLSCTX_INPROC_SERVER,
				__uuidof(IDiaDataSource),
				(void**)&source);
			if (FAILED(hr)) {
				retflag = 3;
				return;
			};

			{
				wchar_t wszFilename[_MAX_PATH];
				wchar_t wszPath[_MAX_PATH];
				mbstowcs(wszFilename, dll_path.c_str(), sizeof(wszFilename) / sizeof(wszFilename[0]));
				mbstowcs(wszPath, sPluginPath.data(), sizeof(wszPath) / sizeof(wszPath[0]));
				hr = source->loadDataForExe(wszFilename, wszPath, NULL);
				if (FAILED(hr)) {
					retflag = 3;
					return;
				};
				logger::info("Found pdb for dll {}", *filename);
			}

			CComPtr<IDiaSession> pSession;
			if (FAILED(source->openSession(&pSession))) {
				retflag = 3;
				return;
			};

			IDiaEnumSymbolsByAddr* pEnumSymbolsByAddr;
			IDiaSymbol* pSymbol;
			ULONG celt = 0;
			if (FAILED(pSession->getSymbolsByAddr(&pEnumSymbolsByAddr))) {
				{
					retflag = 3;
					return;
				};
			}
			if (FAILED(pEnumSymbolsByAddr->symbolByAddr(1, 0, &pSymbol))) {
				pEnumSymbolsByAddr->Release();
				{
					retflag = 3;
					return;
				};
			}
			do {
				const auto rva = 0;
				std::string_view a_name = *filename;
				uintptr_t a_offset = 0;
				std::string result = "";
				result = processSymbol(pSymbol, pSession, rva, a_name, a_offset, result);
				logger::info("{}", result);
				pSymbol->Release();
				if (FAILED(pEnumSymbolsByAddr->Next(1, &pSymbol, &celt))) {
					pEnumSymbolsByAddr->Release();
					break;
				}
			} while (celt == 1);
			pEnumSymbolsByAddr->Release();
		}
	}
}
