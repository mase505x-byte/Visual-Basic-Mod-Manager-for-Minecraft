#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace modmanager {

struct Mod {
	std::string name;
	bool enabled;
};

// Stores mods in <root>/enabled and <root>/disabled. Mod names are restricted
// to safe filename characters so operations cannot escape the managed folders.
class Manager {
public:
	explicit Manager(std::filesystem::path root) : root_(std::move(root)) {}

	std::error_code initialize() const {
		std::error_code ec;
		std::filesystem::create_directories(enabled_dir(), ec);
		if (ec) return ec;
		std::filesystem::create_directories(disabled_dir(), ec);
		return ec;
	}

	std::error_code install(const std::string& name,
							const std::filesystem::path& source) const {
		if (!valid_name(name)) return std::make_error_code(std::errc::invalid_argument);
		std::error_code ec;
		if (!std::filesystem::is_directory(source, ec))
			return ec ? ec : std::make_error_code(std::errc::not_a_directory);
		if (contains(name, ec))
			return ec ? ec : std::make_error_code(std::errc::file_exists);
		if (ec) return ec;
		std::filesystem::create_directories(disabled_dir(), ec);
		if (ec) return ec;
		std::filesystem::copy(source, disabled_dir() / name,
							  std::filesystem::copy_options::recursive, ec);
		return ec;
	}

	std::error_code uninstall(const std::string& name) const {
		if (!valid_name(name)) return std::make_error_code(std::errc::invalid_argument);
		std::error_code ec;
		const auto path = find_mod(name, ec);
		if (ec) return ec;
		if (path.empty()) return std::make_error_code(std::errc::no_such_file_or_directory);
		std::filesystem::remove_all(path, ec);
		return ec;
	}

	std::error_code enable(const std::string& name) const { return move_mod(name, false); }
	std::error_code disable(const std::string& name) const { return move_mod(name, true); }

	std::vector<Mod> list(std::error_code& ec) const {
		std::vector<Mod> result;
		ec.clear();
		append_directory(enabled_dir(), true, result, ec);
		if (!ec) append_directory(disabled_dir(), false, result, ec);
		if (ec) result.clear();
		return result;
	}

	const std::filesystem::path& root() const noexcept { return root_; }

private:
	std::filesystem::path root_;

	std::filesystem::path enabled_dir() const { return root_ / "enabled"; }
	std::filesystem::path disabled_dir() const { return root_ / "disabled"; }

	static bool valid_name(const std::string& name) {
		if (name.empty() || name == "." || name == "..") return false;
		for (unsigned char c : name) {
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
				return false;
		}
		return true;
	}

	bool contains(const std::string& name, std::error_code& ec) const {
		const bool in_enabled = std::filesystem::exists(enabled_dir() / name, ec);
		if (ec || in_enabled) return in_enabled;
		return std::filesystem::exists(disabled_dir() / name, ec);
	}

	std::filesystem::path find_mod(const std::string& name, std::error_code& ec) const {
		const auto enabled = enabled_dir() / name;
		if (std::filesystem::exists(enabled, ec)) return enabled;
		if (ec) return {};
		const auto disabled = disabled_dir() / name;
		return std::filesystem::exists(disabled, ec) ? disabled : std::filesystem::path{};
	}

	std::error_code move_mod(const std::string& name, bool turn_on) const {
		if (!valid_name(name)) return std::make_error_code(std::errc::invalid_argument);
		const auto from = (turn_on ? disabled_dir() : enabled_dir()) / name;
		const auto to = (turn_on ? enabled_dir() : disabled_dir()) / name;
		std::error_code ec;
		if (!std::filesystem::exists(from, ec))
			return ec ? ec : std::make_error_code(std::errc::no_such_file_or_directory);
		if (std::filesystem::exists(to, ec))
			return ec ? ec : std::make_error_code(std::errc::file_exists);
		std::filesystem::create_directories(to.parent_path(), ec);
		if (ec) return ec;
		std::filesystem::rename(from, to, ec);
		return ec;
	}

	static void append_directory(const std::filesystem::path& dir, bool enabled,
								 std::vector<Mod>& result, std::error_code& ec) {
		if (!std::filesystem::exists(dir, ec) || ec) return;
		for (std::filesystem::directory_iterator it(dir, ec), end;
			 !ec && it != end; it.increment(ec)) {
			if (it->is_directory(ec) && !ec)
				result.push_back({it->path().filename().string(), enabled});
		}
	}
};

} // namespace modmanager
