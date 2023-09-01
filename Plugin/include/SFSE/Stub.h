#pragma once

#include "DKUtil/Hook.hpp"
#include "SFSE/PluginAPI.h"

#define SFSEAPI __cdecl

// interface
namespace SFSE
{
	class LoadInterface
	{
	public:
		inline static auto HANDLE = static_cast<PluginHandle>(-1);

		enum : std::uint32_t
		{
			kInvalid = 0,
			kMessaging,
			kTrampoline,

			kTotal,
		};

		[[nodiscard]] constexpr auto RuntimeVersion() const { return GetProxy()->runtimeVersion; }
		[[nodiscard]] constexpr auto SFSEVersion() const { return GetProxy()->sfseVersion; }
		[[nodiscard]] constexpr auto GetPluginHandle() const { return GetProxy()->GetPluginHandle(); };
		[[nodiscard]] constexpr const auto* GetPluginInfo(const char* a_name) const { return GetProxy()->GetPluginInfo(a_name); }
		template <typename T>
		[[nodiscard]] constexpr T* QueryInterface(std::uint32_t a_id) const
		{
			return std::bit_cast<T*>(GetProxy()->QueryInterface(a_id));
		};

	protected:
		constexpr const SFSEInterface* GetProxy() const
		{
			assert(this);
			return std::bit_cast<const SFSEInterface*>(this);
		}
	};

	namespace detail
	{
		inline static LoadInterface* storage = nullptr;
	}
}  // namespace SFSE

// stubs
namespace SFSE
{
	inline void Init(LoadInterface* a_intfc) noexcept
	{
		if (!a_intfc) {
			FATAL("SFSEInterface is null"sv);
		}

		detail::storage = std::launder(a_intfc);
		detail::storage->HANDLE = detail::storage->GetPluginHandle();
	}

	[[nodiscard]] inline constexpr const auto* GetLoadInterface() noexcept { return detail::storage; }
	[[nodiscard]] inline constexpr const auto* GetTrampolineInterface() noexcept
	{
		return GetLoadInterface()->QueryInterface<SFSETrampolineInterface>(LoadInterface::kMessaging);
	}
	[[nodiscard]] inline constexpr const auto* GetMessagingInterface() noexcept
	{
		return GetLoadInterface()->QueryInterface<SFSEMessagingInterface>(LoadInterface::kTrampoline);
	}

	[[nodiscard]] inline auto& GetTrampoline() noexcept { return DKUtil::Hook::Trampoline::GetTrampoline(); }

	void* AllocTrampoline(std::size_t a_size, bool a_useSFSEReserve = true)
	{
		if (auto intfc = GetTrampolineInterface(); intfc && a_useSFSEReserve) {
			auto* mem = intfc->AllocateFromBranchPool(detail::storage->HANDLE, a_size);
			if (mem) {
				return mem;
			} else {
				WARN(
					"requesting allocation from SFSE branch pool failed\n"
					"falling back to local trampoline");
			}
		}

		auto& trampoline = GetTrampoline();
		auto* mem = trampoline.PageAlloc(a_size);
		if (mem) {
			return mem;
		} else {
			FATAL(
				"failed to allocate any memory from either branch pool or local trampoline\n"
				"this is fatal!\nSize : {}",
				a_size);
			std::unreachable();
		}
	}
}  // namespace SFSE

// versiondata
namespace SFSE
{
	struct PluginVersionData
	{
	public:
		enum
		{
			kVersion = 1,
		};

		constexpr void PluginVersion(std::uint32_t a_version) noexcept { pluginVersion = a_version; }
		constexpr void PluginName(std::string_view a_plugin) noexcept { SetCharBuffer(a_plugin, std::span{ pluginName }); }
		constexpr void AuthorName(std::string_view a_name) noexcept { SetCharBuffer(a_name, std::span{ author }); }
		constexpr void UsesSigScanning(bool a_value) noexcept { addressIndependence = !a_value; }
		constexpr void UsesAddressLibrary(bool a_value) noexcept { addressIndependence = a_value; }
		constexpr void HasNoStructUse(bool a_value) noexcept { structureIndependence = !a_value; }
		constexpr void IsLayoutDependent(bool a_value) noexcept { structureIndependence = a_value; }
		constexpr void CompatibleVersions(std::initializer_list<std::uint32_t> a_versions) noexcept
		{
			assert(a_versions.size() < std::size(compatibleVersions) - 1);
			std::ranges::copy(a_versions, compatibleVersions);
		}
		constexpr void MinimumRequiredXSEVersion(std::uint32_t a_version) noexcept { xseMinimum = a_version; }

		const std::uint32_t dataVersion{ kVersion };  // shipped with xse
		std::uint32_t pluginVersion = 0;              // version number of your plugin
		char pluginName[256] = {};                    // null-terminated ASCII plugin name (please make this recognizable to users)
		char author[256] = {};                        // null-terminated ASCII plugin author name
		std::uint32_t addressIndependence;            // describe how you find your addressese using the kAddressIndependence_ enums
		std::uint32_t structureIndependence;          // describe how you handle structure layout using the kStructureIndependence_ enums
		std::uint32_t compatibleVersions[16] = {};    // list of compatible versions
		std::uint32_t xseMinimum = 0;                 // minimum version of the script extender required
		const std::uint32_t reservedNonBreaking = 0;  // set to 0
		const std::uint32_t reservedBreaking = 0;     // set to 0

	private:
		static constexpr void SetCharBuffer(
			std::string_view a_src,
			std::span<char> a_dst) noexcept
		{
			assert(a_src.size() < a_dst.size());
			std::ranges::fill(a_dst, '\0');
			std::ranges::copy(a_src, a_dst.begin());
		}
	};
	static_assert(offsetof(PluginVersionData, dataVersion) == 0x000);
	static_assert(offsetof(PluginVersionData, pluginVersion) == 0x004);
	static_assert(offsetof(PluginVersionData, pluginName) == 0x008);
	static_assert(offsetof(PluginVersionData, author) == 0x108);
	static_assert(offsetof(PluginVersionData, addressIndependence) == 0x208);
	static_assert(offsetof(PluginVersionData, structureIndependence) == 0x20C);
	static_assert(offsetof(PluginVersionData, compatibleVersions) == 0x210);
	static_assert(offsetof(PluginVersionData, xseMinimum) == 0x250);
	static_assert(offsetof(PluginVersionData, reservedNonBreaking) == 0x254);
	static_assert(offsetof(PluginVersionData, reservedBreaking) == 0x258);
	static_assert(sizeof(PluginVersionData) == 0x25C);
}  // namespace SFSE