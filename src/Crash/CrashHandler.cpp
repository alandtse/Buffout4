#include "CrashHandler.h"

#include "Crash/Introspection/Introspection.h"
#include "Crash/Modules/ModuleHandler.h"

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

#include <winternl.h>

namespace Crash
{
	Callstack::Callstack(const ::EXCEPTION_RECORD& a_except) noexcept
	{
		const auto exceptionAddress = reinterpret_cast<std::uintptr_t>(a_except.ExceptionAddress);
		auto it = std::find_if(
			_stacktrace.cbegin(),
			_stacktrace.cend(),
			[&](auto&& a_elem) noexcept {
				return reinterpret_cast<std::uintptr_t>(a_elem.address()) == exceptionAddress;
			});

		if (it == _stacktrace.cend()) {
			it = _stacktrace.cbegin();
		}

		_frames = stl::make_span(it, _stacktrace.cend());
	}

	void Callstack::print(
		std::shared_ptr<spdlog::logger> a_log,
		stl::span<const module_pointer> a_modules) const noexcept
	{
		assert(a_log != nullptr);
		print_probable_callstack(a_log, a_modules);
	}

	std::string Callstack::get_size_string(std::size_t a_size) noexcept
	{
		return fmt::to_string(
			fmt::to_string(a_size - 1)
				.length());
	}

	std::string Callstack::get_format(std::size_t a_nameWidth) const noexcept
	{
		return "\t[{:>"s +
			   get_size_string(_frames.size()) +
			   "}] 0x{:012X} {:>"s +
			   fmt::to_string(a_nameWidth) +
			   "}{}"s;
	}

	void Callstack::print_probable_callstack(
		std::shared_ptr<spdlog::logger> a_log,
		stl::span<const module_pointer> a_modules) const noexcept
	{
		assert(a_log != nullptr);
		a_log->critical("PROBABLE CALL STACK:"sv);

		std::vector<const Modules::Module*> moduleStack;
		moduleStack.reserve(_frames.size());
		for (const auto& frame : _frames) {
			const auto it = std::lower_bound(
				a_modules.rbegin(),
				a_modules.rend(),
				reinterpret_cast<std::uintptr_t>(frame.address()),
				[](auto&& a_lhs, auto&& a_rhs) noexcept {
					return a_lhs->address() >= a_rhs;
				});
			if (it != a_modules.rend() && (*it)->in_range(frame.address())) {
				moduleStack.push_back(it->get());
			} else {
				moduleStack.push_back(nullptr);
			}
		}

		const auto format = get_format([&]() noexcept {
			std::size_t max = 0;
			std::for_each(
				moduleStack.begin(),
				moduleStack.end(),
				[&](auto&& a_elem) noexcept {
					max = a_elem ? std::max(max, a_elem->name().length()) : max;
				});
			return max;
		}());

		for (std::size_t i = 0; i < _frames.size(); ++i) {
			const auto mod = moduleStack[i];
			const auto& frame = _frames[i];
			a_log->critical(
				format,
				i,
				reinterpret_cast<std::uintptr_t>(frame.address()),
				(mod ? mod->name() : ""sv),
				(mod ? mod->frame_info(frame) : ""s));
		}
	}

	void Callstack::print_raw_callstack(std::shared_ptr<spdlog::logger> a_log) const noexcept
	{
		assert(a_log != nullptr);
		a_log->critical("RAW CALL STACK:");

		const auto format =
			"\t[{:>"s +
			get_size_string(_stacktrace.size()) +
			"}] 0x{:X}"s;

		for (std::size_t i = 0; i < _stacktrace.size(); ++i) {
			a_log->critical(
				format,
				i,
				reinterpret_cast<std::uintptr_t>(_stacktrace[i].address()));
		}
	}

	namespace
	{
		[[nodiscard]] std::shared_ptr<spdlog::logger> get_log() noexcept
		{
			auto path = logger::log_directory();
			if (!path) {
				stl::report_and_fail("failed to find standard log directory"sv);
			}

			const auto time = std::time(nullptr);
			std::tm localTime;
			if (gmtime_s(std::addressof(localTime), std::addressof(time)) != 0) {
				stl::report_and_fail("failed to get current time"sv);
			}

			std::stringstream buf;
			buf << "crash-"sv << std::put_time(std::addressof(localTime), "%Y-%m-%d-%H-%M-%S") << ".log"sv;
			*path /= buf.str();

			auto sink = std::make_shared<spdlog::sinks::basic_file_sink_st>(path->string(), true);
			const auto log = std::make_shared<spdlog::logger>("crash log"s, std::move(sink));
			log->set_pattern("%v"s);
			log->set_level(spdlog::level::trace);
			log->flush_on(spdlog::level::off);

			return log;
		}

#define EXCEPTION_CASE(a_code)                                               \
	case a_code:                                                             \
		a_log->critical(                                                     \
			FMT_STRING("Unhandled exception \"{}\" at 0x{:X}"),              \
			#a_code##sv,                                                     \
			reinterpret_cast<std::uintptr_t>(a_exception.ExceptionAddress)); \
		break

		void print_exception(
			std::shared_ptr<spdlog::logger> a_log,
			const ::EXCEPTION_RECORD& a_exception) noexcept
		{
			assert(a_log != nullptr);

			switch (a_exception.ExceptionCode) {
				EXCEPTION_CASE(EXCEPTION_ACCESS_VIOLATION);
				EXCEPTION_CASE(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
				EXCEPTION_CASE(EXCEPTION_BREAKPOINT);
				EXCEPTION_CASE(EXCEPTION_DATATYPE_MISALIGNMENT);
				EXCEPTION_CASE(EXCEPTION_FLT_DENORMAL_OPERAND);
				EXCEPTION_CASE(EXCEPTION_FLT_DIVIDE_BY_ZERO);
				EXCEPTION_CASE(EXCEPTION_FLT_INEXACT_RESULT);
				EXCEPTION_CASE(EXCEPTION_FLT_INVALID_OPERATION);
				EXCEPTION_CASE(EXCEPTION_FLT_OVERFLOW);
				EXCEPTION_CASE(EXCEPTION_FLT_STACK_CHECK);
				EXCEPTION_CASE(EXCEPTION_FLT_UNDERFLOW);
				EXCEPTION_CASE(EXCEPTION_ILLEGAL_INSTRUCTION);
				EXCEPTION_CASE(EXCEPTION_IN_PAGE_ERROR);
				EXCEPTION_CASE(EXCEPTION_INT_DIVIDE_BY_ZERO);
				EXCEPTION_CASE(EXCEPTION_INT_OVERFLOW);
				EXCEPTION_CASE(EXCEPTION_INVALID_DISPOSITION);
				EXCEPTION_CASE(EXCEPTION_NONCONTINUABLE_EXCEPTION);
				EXCEPTION_CASE(EXCEPTION_PRIV_INSTRUCTION);
				EXCEPTION_CASE(EXCEPTION_SINGLE_STEP);
				EXCEPTION_CASE(EXCEPTION_STACK_OVERFLOW);
			default:
				a_log->critical(
					FMT_STRING("Unhandled exception at 0x{:X}"),
					reinterpret_cast<std::uintptr_t>(a_exception.ExceptionAddress));
				break;
			}
		}

#undef EXCEPTION_CASE

		void print_modules(
			std::shared_ptr<spdlog::logger> a_log,
			stl::span<const module_pointer> a_modules) noexcept
		{
			assert(a_log != nullptr);
			a_log->critical("MODULES:"sv);

			const auto format = [&]() noexcept {
				const auto width = [&]() noexcept {
					std::size_t max = 0;
					std::for_each(
						a_modules.begin(),
						a_modules.end(),
						[&](auto&& a_elem) {
							max = std::max(max, a_elem->name().length());
						});
					return max;
				}();

				return "\t{:<"s +
					   fmt::to_string(width) +
					   "} 0x{:012X}"s;
			}();

			for (const auto& mod : a_modules) {
				a_log->critical(
					format,
					mod->name(),
					mod->address());
			}
		}

		void print_plugins(std::shared_ptr<spdlog::logger> a_log) noexcept
		{
			assert(a_log != nullptr);
			a_log->critical("PLUGINS:"sv);

			const auto datahandler = RE::TESDataHandler::GetSingleton();
			if (datahandler) {
				const auto& [files, smallfiles] = datahandler->compiledFileCollection;

				const auto fileFormat = [&](const auto& a_files) {
					return "\t[{:>02X}]{:"s +
						   (!a_files.empty() ? "5"s : "1"s) +
						   "}{}"s;
				}(smallfiles);

				for (const auto file : files) {
					a_log->critical(
						fileFormat,
						file->GetCompileIndex(),
						"",
						file->GetFilename());
				}

				for (const auto file : smallfiles) {
					a_log->critical(
						FMT_STRING("\t[FE:{:>03X}] {}"),
						file->GetSmallFileCompileIndex(),
						file->GetFilename());
				}
			}
		}

		void print_registers(
			std::shared_ptr<spdlog::logger> a_log,
			const ::CONTEXT& a_context,
			stl::span<const module_pointer> a_modules) noexcept
		{
			assert(a_log != nullptr);
			a_log->critical("REGISTERS:"sv);

			const std::array regs{
				std::make_pair("RAX"sv, a_context.Rax),
				std::make_pair("RCX"sv, a_context.Rcx),
				std::make_pair("RDX"sv, a_context.Rdx),
				std::make_pair("RBX"sv, a_context.Rbx),
				std::make_pair("RSP"sv, a_context.Rsp),
				std::make_pair("RBP"sv, a_context.Rbp),
				std::make_pair("RSI"sv, a_context.Rsi),
				std::make_pair("RDI"sv, a_context.Rdi),
				std::make_pair("R8"sv, a_context.R8),
				std::make_pair("R9"sv, a_context.R9),
				std::make_pair("R10"sv, a_context.R10),
				std::make_pair("R11"sv, a_context.R11),
				std::make_pair("R12"sv, a_context.R12),
				std::make_pair("R13"sv, a_context.R13),
				std::make_pair("R14"sv, a_context.R14),
				std::make_pair("R15"sv, a_context.R15),
			};

			std::array<std::size_t, regs.size()> todo;
			for (std::size_t i = 0; i < regs.size(); ++i) {
				todo[i] = regs[i].second;
			}
			const auto analysis = Introspection::analyze_data(todo, a_modules);
			for (std::size_t i = 0; i < regs.size(); ++i) {
				const auto& [name, reg] = regs[i];
				a_log->critical(
					FMT_STRING("\t{:<3} 0x{:<16X} {}"),
					name,
					reg,
					analysis[i]);
			}
		}

		void print_stack(
			std::shared_ptr<spdlog::logger> a_log,
			const ::CONTEXT& a_context,
			stl::span<const module_pointer> a_modules) noexcept
		{
			assert(a_log != nullptr);
			a_log->critical("STACK:"sv);

			const auto tib = reinterpret_cast<const ::NT_TIB*>(::NtCurrentTeb());
			const auto base = tib ? static_cast<const std::size_t*>(tib->StackBase) : nullptr;
			if (!base) {
				a_log->critical("\tFAILED TO READ TIB"sv);
			} else {
				const auto rsp = reinterpret_cast<const std::size_t*>(a_context.Rsp);
				stl::span stack{ rsp, base };

				const auto format = [&]() noexcept {
					return "\t[RSP+{:<"s +
						   fmt::to_string(
							   fmt::format(FMT_STRING("{:X}"), (stack.size() - 1) * sizeof(std::size_t))
								   .length()) +
						   "X}] 0x{:<16X} {}"s;
				}();

				constexpr std::size_t blockSize = 1000;
				std::size_t idx = 0;
				for (std::size_t off = 0; off < stack.size(); off += blockSize) {
					const auto analysis =
						Introspection::analyze_data(
							stack.subspan(off, std::min<std::size_t>(stack.size() - off, blockSize)),
							a_modules);
					for (const auto& data : analysis) {
						a_log->critical(
							format,
							idx * sizeof(std::size_t),
							stack[idx],
							data);
						++idx;
					}
				}
			}
		}

		void print_sysinfo(
			std::shared_ptr<spdlog::logger> a_log) noexcept
		{
			assert(a_log != nullptr);
			a_log->critical("SYSTEM SPECS:"sv);

			const auto os = iware::system::OS_info();
			a_log->critical(
				FMT_STRING("\tOS: {} v{}.{}.{}"),
				os.full_name,
				os.major,
				os.minor,
				os.patch);

			a_log->critical(
				FMT_STRING("\tCPU: {} {}"),
				iware::cpu::vendor(),
				iware::cpu::model_name());

			const auto vendor = [](iware::gpu::vendor_t a_vendor) {
				using vendor_t = iware::gpu::vendor_t;
				switch (a_vendor) {
				case vendor_t::intel:
					return "Intel"sv;
				case vendor_t::amd:
					return "AMD"sv;
				case vendor_t::nvidia:
					return "Nvidia"sv;
				case vendor_t::microsoft:
					return "Microsoft"sv;
				case vendor_t::qualcomm:
					return "Qualcomm"sv;
				case vendor_t::unknown:
				default:
					return "Unknown"sv;
				}
			};

			const auto gpus = iware::gpu::device_properties();
			for (std::size_t i = 0; i < gpus.size(); ++i) {
				const auto& gpu = gpus[i];
				a_log->critical(
					FMT_STRING("\tGPU #{}: {} {}"),
					i + 1,
					vendor(gpu.vendor),
					gpu.name);
			}

			const auto gibibyte = [](std::uint64_t a_bytes) {
				constexpr double factor = 1024 * 1024 * 1024;
				return static_cast<double>(a_bytes) / factor;
			};

			const auto mem = iware::system::memory();
			a_log->critical(
				FMT_STRING("\tPHYSICAL MEMORY: {:.02f} GB/{:.02f} GB"),
				gibibyte(mem.physical_total - mem.physical_available),
				gibibyte(mem.physical_total));
		}

#define SETTING_CASE(a_enum, a_type)                                                \
	case toml::node_type::a_enum:                                                   \
		if (const auto ptr = dynamic_cast<const AutoTOML::a_type*>(setting); ptr) { \
			value = fmt::format(                                                    \
				FMT_STRING("{}"),                                                   \
				ptr->get());                                                        \
		}                                                                           \
		break

		void print_settings(
			std::shared_ptr<spdlog::logger> a_log) noexcept
		{
			assert(a_log != nullptr);
			a_log->critical("SETTINGS:"sv);

			const auto groups = []() {
				const auto& src = AutoTOML::ISetting::get_settings();
				boost::container::map<
					std::string_view,
					std::vector<AutoTOML::ISetting*>>
					groups;
				std::for_each(
					src.begin(),
					src.end(),
					[&](auto&& a_elem) {
						assert(a_elem != nullptr);
						const auto it = groups.emplace(a_elem->group(), 0).first;
						assert(it != groups.end());
						it->second.push_back(a_elem);
					});
				std::for_each(
					groups.begin(),
					groups.end(),
					[](auto& a_elem) {
						std::sort(
							a_elem.second.begin(),
							a_elem.second.end(),
							[](auto&& a_lhs, auto&& a_rhs) {
								assert(a_lhs != nullptr && a_rhs != nullptr);
								return a_lhs->key() < a_rhs->key();
							});
					});
				return groups;
			}();

			std::string value;
			for (const auto& [group, settings] : groups) {
				assert(!settings.empty());
				a_log->critical(
					FMT_STRING("\t[{}]"),
					group);

				for (const auto setting : settings) {
					assert(setting != nullptr);

					switch (setting->type()) {
						SETTING_CASE(boolean, bSetting);
						SETTING_CASE(floating_point, fSetting);
						SETTING_CASE(integer, iSetting);
						SETTING_CASE(string, sSetting);
					default:
						value = "UNKNOWN"sv;
						break;
					}

					a_log->critical(
						FMT_STRING("\t\t{}: {}"),
						setting->key(),
						value);
				}
			}
		}

#undef SETTING_CASE

		std::int32_t __stdcall UnhandledExceptions(::EXCEPTION_POINTERS* a_exception) noexcept
		{
#ifndef NDEBUG
			for (; !WinAPI::IsDebuggerPresent();) {}
#endif

			const auto modules = Modules::get_loaded_modules();
			const auto cmodules = stl::make_span(modules.begin(), modules.end());
			const auto log = get_log();

			const auto print = [&](auto&& a_functor) noexcept {
				log->critical(""sv);
				a_functor();
				log->flush();
			};

			log->critical("v{}", Version::NAME);
			log->flush();

			print([&]() noexcept { print_exception(log, *a_exception->ExceptionRecord); });
			print([&]() noexcept { print_settings(log); });
			print([&]() noexcept { print_sysinfo(log); });

			print([&]() noexcept {
				const Callstack callstack{ *a_exception->ExceptionRecord };
				callstack.print(log, cmodules);
			});

			print([&]() noexcept { print_registers(log, *a_exception->ContextRecord, cmodules); });
			print([&]() noexcept { print_stack(log, *a_exception->ContextRecord, cmodules); });
			print([&]() noexcept { print_modules(log, cmodules); });
			print([&]() noexcept { print_plugins(log); });

			WinAPI::TerminateProcess(
				WinAPI::GetCurrentProcess(),
				EXIT_FAILURE);

			return EXCEPTION_CONTINUE_SEARCH;
		}

		std::int32_t _stdcall VectoredExceptions(::EXCEPTION_POINTERS*) noexcept
		{
			::SetUnhandledExceptionFilter(
				reinterpret_cast<::LPTOP_LEVEL_EXCEPTION_FILTER>(&UnhandledExceptions));
			return EXCEPTION_CONTINUE_SEARCH;
		}
	}

	void Install()
	{
		const auto success =
			::AddVectoredExceptionHandler(
				1,
				reinterpret_cast<::PVECTORED_EXCEPTION_HANDLER>(&VectoredExceptions));
		if (success == nullptr) {
			stl::report_and_fail("failed to install vectored exception handler"sv);
		}
		logger::info("installed crash handlers"sv);
	}
}
