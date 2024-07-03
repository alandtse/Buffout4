#include "Compatibility/Compatibility.h"
#include "Crash/CrashHandler.h"
#include "Fixes/Fixes.h"
#include "Patches/Patches.h"
#include "Warnings/Warnings.h"

namespace
{
	bool g_preloaded = false;

	void AllocTrampoline()
	{
		auto& trampoline = F4SE::GetTrampoline();
		if (trampoline.empty()) {
			F4SE::AllocTrampoline(1u << 9);
		}
	}

	void PostInit()
	{
		const auto task = F4SE::GetTaskInterface();
		task->AddTask([]() {
			Fixes::PostInit();
			if (REL::Module::IsF4()) {
				Patches::PostInit();
			}
		});
	}

	void F4SEAPI MessageHandler(F4SE::MessagingInterface::Message* a_message)
	{
		switch (a_message->type) {
		case F4SE::MessagingInterface::kPostPostLoad:
			if (REL::Module::IsF4()) {
				Compatibility::Install();
			}
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
			{
				Fixes::PostLoadGame();
				break;
			}
		case F4SE::MessagingInterface::kGameLoaded:
			{
				static std::once_flag guard;
				std::call_once(guard, PostInit);
			}
			break;
		case F4SE::MessagingInterface::kGameDataReady:
			{
				Fixes::GameDataReady();
			}
			break;
		}
	}

	void OpenLog()
	{
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path) {
			stl::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format("{}.log"sv, "Buffout4"sv);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
		const auto level = spdlog::level::trace;
#else
		const auto level = spdlog::level::trace;
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %T.%e][%-16s:%-4#][%L]: %v"s);
	}
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	if (!g_preloaded) {
		stl::report_and_fail("The plugin preloader is not installed or did not run correctly"sv);
	}

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION[0];

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor"sv);
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < (REL::Module::IsF4() ? F4SE::RUNTIME_LATEST : F4SE::RUNTIME_LATEST_VR)) {
		logger::critical("unsupported runtime v{}"sv, ver.string());
		return false;
	}
	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	AllocTrampoline();
	F4SE::Init(a_f4se);
	spdlog::set_pattern("[%Y-%m-%d %T.%e][%-16s:%-4#][%L]: %v"s);
	logger::info("NOTE: This is not a crashlog. Crashlogs have the name crash-[TIMESTAMP].log");
	logger::info("Buffout 4 v{}.{}.{} {} {} is loading"sv, Plugin::VERSION[0], Plugin::VERSION[1], Plugin::VERSION[2], __DATE__, __TIME__);
	const auto messaging = F4SE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		return false;
	}

	return true;
}

F4SE_EXPORT constinit auto F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData data{};

	data.PluginName(Plugin::NAME.data());
	data.PluginVersion(Plugin::VERSION);
	data.AuthorName("alandtse");
	data.UsesAddressLibrary(true);
	data.UsesSigScanning(false);
	data.IsLayoutDependent(true);
	data.HasNoStructUse(false);
	data.CompatibleVersions({ F4SE::RUNTIME_LATEST, F4SE::RUNTIME_LATEST_VR });

	return data;
}();

#define WIN32_LEAN_AND_MEAN

#define NOGDICAPMASKS
#define NOVIRTUALKEYCODES
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOGDI
#define NOKERNEL
#define NOUSER
#define NONLS
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX

#include <Windows.h>

namespace
{
	void* GetIAT(
		void* a_module,
		std::string_view a_dll,
		std::string_view a_function)
	{
		assert(a_module != nullptr);
		const auto dosHeader = reinterpret_cast<::IMAGE_DOS_HEADER*>(a_module);
		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
			stl::report_and_fail("invalid dos header"sv);
		}

		const auto ntHeader = stl::adjust_pointer<::IMAGE_NT_HEADERS>(dosHeader, dosHeader->e_lfanew);
		const auto& dataDir = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		const auto importDesc = stl::adjust_pointer<::IMAGE_IMPORT_DESCRIPTOR>(dosHeader, dataDir.VirtualAddress);

		for (auto import = importDesc; import->Characteristics != 0; ++import) {
			const auto name = stl::adjust_pointer<const char>(dosHeader, import->Name);
			if (_stricmp(a_dll.data(), name) != 0) {
				continue;
			}

			const auto thunk = stl::adjust_pointer<::IMAGE_THUNK_DATA>(dosHeader, import->OriginalFirstThunk);
			for (std::size_t i = 0; thunk[i].u1.Ordinal; ++i) {
				if (IMAGE_SNAP_BY_ORDINAL(thunk[i].u1.Ordinal)) {
					continue;
				}

				const auto importByName = stl::adjust_pointer<IMAGE_IMPORT_BY_NAME>(dosHeader, thunk[i].u1.AddressOfData);
				if (_stricmp(a_function.data(), importByName->Name) == 0) {
					return stl::adjust_pointer<::IMAGE_THUNK_DATA>(dosHeader, import->FirstThunk) + i;
				}
			}
		}

		stl::report_and_fail(
			fmt::format(
				"failed to find {}!{}"sv,
				a_dll,
				a_function));
	}

	std::uintptr_t PatchIAT(
		void* a_module,
		std::uintptr_t a_newFunc,
		std::string_view a_dll,
		std::string_view a_function)
	{
		const auto oldFunc = GetIAT(a_module, a_dll, a_function);
		const auto original = *reinterpret_cast<std::uintptr_t*>(oldFunc);
		REL::safe_write(reinterpret_cast<std::uintptr_t>(oldFunc), a_newFunc);
		return original;
	}

	struct initterm
	{
		static void thunk(std::uintptr_t* a_first, std::uintptr_t* a_last)
		{
			void (*const proxy)() = []() {
				AllocTrampoline();
				Crash::Install();
				Fixes::PreLoad();
				Patches::PreLoad();
				Warnings::PreLoad();
			};

			std::vector<std::uintptr_t> cache(a_first, a_last);
			const auto pos = [&]() {
				const REL::Relocation<std::uintptr_t> preCppInit{ REL::ID(1440502) };
				const auto it = std::find(cache.begin(), cache.end(), preCppInit.address());
				return it != cache.end() ? it + 1 :
				       !cache.empty()    ? cache.begin() + 1 :
				                           cache.end();
			}();
			cache.insert(pos, reinterpret_cast<std::uintptr_t>(proxy));

			func(
				std::to_address(cache.begin()),
				std::to_address(cache.end()));
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};
}

::BOOL WINAPI DllMain(::HINSTANCE, ::DWORD a_reason, ::LPVOID)
{
	enum class NativeStartupState
	{
		kUninitialized,
		kInitializing,
		kInitialized
	};

#ifndef NDEBUG
	for (; !::IsDebuggerPresent();) {}
#endif

	if (a_reason == DLL_PROCESS_ATTACH) {
		if (::GetModuleHandleW(L"CreationKit.exe")) {
			return FALSE;
		}

		OpenLog();
		Settings::load();

		REL::Relocation<NativeStartupState*> startupState{ REL::ID(3070) };
		if (*startupState != NativeStartupState::kUninitialized) {
			stl::report_and_fail(
				fmt::format(
					"{} has loaded too late. Try adjusting the plugin preloader load method."sv,
					Plugin::NAME));
		}

		initterm::func = PatchIAT(
			REL::Module::get().pointer(),
			reinterpret_cast<std::uintptr_t>(initterm::thunk),
			"MSVCR110.dll"sv,
			"_initterm"sv);

		g_preloaded = true;
	}

	return TRUE;
}
