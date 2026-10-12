#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Engine/materializer adapters let this manager remain independent of their APIs.
class ModManager {
public:
	struct Mod {
		std::string id;
		std::string source;
		std::vector<std::string> dependencies;
	};

	using Materialize = std::function<bool(const Mod&, std::string&)>;
	using EngineLoad = std::function<bool(const Mod&, const std::string&)>;
	using EngineUnload = std::function<void(const Mod&)>;

	ModManager(Materialize materialize, EngineLoad load, EngineUnload unload)
		: materialize_(std::move(materialize)), load_(std::move(load)),
		  unload_(std::move(unload)) {}

	bool add(Mod mod) {
		if (mod.id.empty() || mods_.count(mod.id))
			return false;
		mods_.emplace(mod.id, Entry{std::move(mod), {}, false});
		return true;
	}

	bool remove(const std::string& id) {
		auto it = mods_.find(id);
		if (it == mods_.end() || it->second.enabled || hasEnabledDependents(id))
			return false;
		mods_.erase(it);
		return true;
	}

	bool enable(const std::string& id) {
		auto it = mods_.find(id);
		if (it == mods_.end())
			return false;
		Entry& entry = it->second;
		if (entry.enabled)
			return true;
		for (const auto& dependency : entry.mod.dependencies) {
			const auto dep = mods_.find(dependency);
			if (dep == mods_.end() || !dep->second.enabled)
				return false;
		}

		std::string output;
		if (!materialize_ || !load_ || !materialize_(entry.mod, output) ||
			!load_(entry.mod, output))
			return false;
		entry.materialized = std::move(output);
		entry.enabled = true;
		return true;
	}

	bool disable(const std::string& id) {
		auto it = mods_.find(id);
		if (it == mods_.end() || !it->second.enabled || hasEnabledDependents(id))
			return false;
		if (unload_)
			unload_(it->second.mod);
		it->second.materialized.clear();
		it->second.enabled = false;
		return true;
	}

	bool isEnabled(const std::string& id) const {
		const auto it = mods_.find(id);
		return it != mods_.end() && it->second.enabled;
	}

	const Mod* find(const std::string& id) const {
		const auto it = mods_.find(id);
		return it == mods_.end() ? nullptr : &it->second.mod;
	}

private:
	struct Entry {
		Mod mod;
		std::string materialized;
		bool enabled;
	};

	bool hasEnabledDependents(const std::string& id) const {
		for (const auto& pair : mods_) {
			const Entry& entry = pair.second;
			if (!entry.enabled)
				continue;
			for (const auto& dependency : entry.mod.dependencies) {
				if (dependency == id)
					return true;
			}
		}
		return false;
	}

	std::unordered_map<std::string, Entry> mods_;
	Materialize materialize_;
	EngineLoad load_;
	EngineUnload unload_;
};
