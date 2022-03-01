#include <stdinclude.hpp>
// #include "../../build/strcvtid.hpp"

using namespace std;
using namespace MHotkey;
// using namespace LocalStrCvt;

namespace local
{
	namespace
	{
		std::mutex db_lock;
		unordered_map<size_t, string> text_db;
		std::vector<size_t> str_list;
	}

	string wide_u8(wstring str)
	{
		string result;
		result.resize(str.length() * 4);

		int len = WideCharToMultiByte(CP_UTF8, 0, str.data(), str.length(), result.data(), result.size(), nullptr, nullptr);

		result.resize(len);

		return result;
	}

	void unlocked_load_textdb(const vector<string>* dicts, map<size_t, string>&& staticDict)
	{
		for (auto&& [id, content] : staticDict)
		{
			text_db.emplace(id, std::move(content));
		}

		for (const auto& dict : *dicts)
		{
			if (filesystem::exists(dict))
			{
				std::ifstream dict_stream{ dict };

				if (!dict_stream.is_open())
					continue;

				printf("Reading %s...\n", dict.data());

				rapidjson::IStreamWrapper wrapper{ dict_stream };
				rapidjson::Document document;

				document.ParseStream(wrapper);

				for (auto iter = document.MemberBegin();
					iter != document.MemberEnd(); ++iter)
				{
					auto key = std::stoull(iter->name.GetString());
					auto value = iter->value.GetString();

					text_db.emplace(key, value);
				}

				dict_stream.close();
			}
		}

		printf("loaded %llu localized entries.\n", text_db.size());
		// read_str_config();
	}

	void reload_textdb(const vector<string>* dicts, map<size_t, string>&& staticDict)
	{
		std::unique_lock lock(db_lock);
		text_db.clear();
		unlocked_load_textdb(dicts, std::move(staticDict));
	}

	void load_textdb(const vector<string>* dicts, map<size_t, string>&& staticDict)
	{
		std::unique_lock lock(db_lock);
		unlocked_load_textdb(dicts, std::move(staticDict));
	}

	bool localify_text(size_t hash, string** result)
	{
		std::unique_lock lock(db_lock);
		if (text_db.contains(hash))
		{
			*result = &text_db[hash];
			return true;
		}

		return false;
	}

	Il2CppString* get_localized_string(size_t hash_or_id)
	{
		// printf("calling get_localized_string(size_t hash_or_id): %zu\n", hash_or_id);
		string* result;

		if (local::localify_text(hash_or_id, &result))
		{
			return il2cpp_string_new(result->data());
		}

		return nullptr;
	}

	wstring line_break_replace(Il2CppString* text) {
		wstring _ret = L"";
		wstring tmp = text->start_char;
		bool space_flag = false;  // 后接空格
		
		for (int i = 0; i < tmp.length(); i++) {
			if (tmp[i] != L'\n' && tmp[i] != L'\r') {
				_ret += tmp[i];
			}
			else if (space_flag){
				_ret += L'　';
			}

			space_flag = false;
			if (tmp[i] == L'！' || tmp[i] == L'？') {
				space_flag = true;  // 标记空格
			}
		}

		return _ret;
	}

	Il2CppString* get_localized_string(Il2CppString* str)
	{
		// printf("call get_localized_string(Il2CppString* str): %ls\n", str->start_char);
		string* result;
		wstring result_without_lb = line_break_replace(str);
		
		/*
		if (wcscmp(L"わわわっ、どいてどいてぇー！！", str->start_char) == 0) {
			printf("LOOOOOOOOOOOOOOOOOOOOOOG: 命中 - '%ls'\n", str->start_char);
			// wchar_t rr[] = L"哇, zhemeniuibi";
			string rr = "wa, zhemeniuibi";
			return il2cpp_string_new(rr.data());
		}
		*/

		size_t hash;
		auto hash_with_lb = std::hash<wstring>{}(str->start_char);
		auto hash_without_lb = std::hash<wstring>{}(result_without_lb);
		bool is_without_lb = get_uma_stat();

		if (is_without_lb) {
			hash = hash_without_lb;
		}
		else {
			hash = hash_with_lb;
		}
		
		/*
		wstring nb = L"わわわっ、どいてどいてぇー！！";
		// nb.c_str(); // char*

		size_t len = strlen(nb.c_str()) + 1;
		size_t converted = 0;
		wchar_t* WStr;
		WStr = (wchar_t*)malloc(len * sizeof(wchar_t));

		mbstowcs_s(&converted, WStr, len, nb.c_str(), _TRUNCATE);

		// auto test1 = std::hash<wstring> {}(L"わわわっ、どいてどいてぇー！！");
		auto test1 = std::hash<wstring> {}(WStr);
		printf("haha: %zu\n", test1);
		*/

		Il2CppString* t_with_lp = NULL;
		Il2CppString* t_without_lp = NULL;

		if (local::localify_text(hash_without_lb, &result))
		{
			t_without_lp = il2cpp_string_new(result->data());  // "忽略换行符" 模式的文本
		}

		if (local::localify_text(hash_with_lb, &result))
		{
			t_with_lp = il2cpp_string_new(result->data());  // 原文本
		}

		if (t_with_lp != NULL && t_without_lp != NULL) {
			return is_without_lb ? t_without_lp : t_with_lp;
		}
		else if (t_with_lp != NULL || t_without_lp != NULL) {
			return t_with_lp != NULL ? t_with_lp : t_without_lp;
		}


		if (g_enable_logger && !std::any_of(str_list.begin(), str_list.end(), [hash](size_t hash1) { return hash1 == hash; }))
		{
			
			/*
			if (hash == 13202430879488954923) {
				wprintf(L"LOOOOOOOOOOOOOOOOOOOOOOG: 命中id - '%ls'\n", str->start_char);

				string rr = "zhemeniuibi";
				string* ret = &rr;
				return il2cpp_string_new(ret->data());
			}
			*/

			str_list.push_back(hash);

			logger::write_entry(hash_with_lb, str->start_char);
			if (hash_with_lb != hash_without_lb) {
				logger::write_entry(hash_without_lb, result_without_lb);
			}
			
			// wprintf(L"未命中: %ls(%zu)\n", str->start_char, hash);

		}

		return str;
	}
}
