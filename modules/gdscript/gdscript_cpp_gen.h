/**************************************************************************/
/*  gdscript_cpp_gen.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "gdscript_parser.h"

#include "core/error/error_list.h"
#include "core/io/dir_access.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"

class GDScriptCPPGen : public Object {
	GDCLASS(GDScriptCPPGen, Object);
	Array file_queue;
	String output_folder = "res://.godot/gdscript_cpp";

	void add_file_to_queue(const String &p_file_path) {
		if (file_queue.has(p_file_path)) {
			return; // Already queued
		}
		file_queue.push_back(p_file_path);
	}

	struct GeneratingClass {
		GDScriptParser::ClassNode *root_class = nullptr;
		String class_name;
		String file_path;
		String output_file_name;
		String source_code_cpp;
		Array preload_cache;
		Array includes_cpp;
		int indent_level_cpp = 0;
		String source_code_header;
		Array includes_header;
		int indent_level_header = 0;
		String source_code_buffer;
		Array includes_buffer;
		int indent_level_buffer = 0;

		String gen_line(const String &p_text, const String &p_comment = "", int p_indent_level = 0) {
			ERR_FAIL_COND_V_MSG(p_text.is_empty() && p_comment.is_empty(), "", "Both text and comment are empty in gen_line().");
			ERR_FAIL_COND_V_MSG(p_text.find("\n") != -1, "", "p_text in gen_line() should not contain newlines.");
			ERR_FAIL_COND_V_MSG(p_comment.find("\n") != -1, "", "p_comment in gen_line() should not contain newlines.");
			String indent;
			if (p_indent_level > 0) {
				indent = String("\t").repeat(p_indent_level);
			}
			String result;
			if (!p_comment.is_empty()) {
				result += indent + "// " + p_comment + "\n";
			}
			if (!p_text.is_empty()) {
				result += indent + p_text + "\n";
			}
			return result;
		}

		void push_line_header(const String &p_text, const String &p_comment = "") { source_code_header += gen_line(p_text, p_comment, indent_level_header); }
		void push_line_cpp(const String &p_text, const String &p_comment = "") { source_code_cpp += gen_line(p_text, p_comment, indent_level_cpp); }
		void push_line_buffer(const String &p_text, const String &p_comment = "") { source_code_buffer += gen_line(p_text, p_comment, indent_level_buffer); }
		void push_buffer(const String &p_text) {
			if (source_code_buffer.is_empty() || source_code_buffer[source_code_buffer.length() - 1] == '\n') {
				source_code_buffer += String("\t").repeat(indent_level_buffer);
			}
			source_code_buffer += p_text;
		}
		void end_line_buffer() {
			ERR_FAIL_COND_MSG(source_code_buffer.is_empty(), "Buffer is empty.");
			if (source_code_buffer.ends_with(";")) {
				source_code_buffer += "\n";
			} else if (!source_code_buffer.ends_with(";\n")) {
				source_code_buffer += ";\n";
			}
		}
		void increment_indent_level_cpp() { indent_level_cpp++; }
		void increment_indent_level_header() { indent_level_header++; }
		void increment_indent_level_buffer() { indent_level_buffer++; }

		void decrement_indent_level_header() { indent_level_header--; }
		void decrement_indent_level_cpp() { indent_level_cpp--; }
		void decrement_indent_level_buffer() { indent_level_buffer--; }

		void push_include_cpp(const String &p_include) {
			if (p_include.is_empty()) {
				return;
			}
			// also check header
			if (!includes_header.has(p_include) && !includes_cpp.has(p_include)) {
				includes_cpp.push_back(p_include.strip_edges());
			}
		}

		void push_include_header(const String &p_include) {
			if (p_include.is_empty()) {
				return;
			}
			if (p_include.get_basename() != output_file_name && !includes_header.has(p_include)) {
				includes_header.push_back(p_include.strip_edges());
			}
		}

		void push_include_buffer(const String &p_include) {
			if (p_include.is_empty()) {
				return;
			}
			if (!includes_buffer.has(p_include)) {
				includes_buffer.push_back(p_include.strip_edges());
			}
		}

		void flush_buffer_to_header() {
			for (int i = 0; i < includes_buffer.size(); i++) {
				push_include_header(includes_buffer[i]);
			}
			includes_buffer.clear();
			PackedStringArray lines = source_code_buffer.split("\n");
			for (int i = 0; i < lines.size(); i++) {
				if (!lines[i].is_empty()) {
					push_line_header(lines[i]);
				}
			}
			source_code_buffer = "";
		}

		void flush_buffer_to_cpp() {
			for (int i = 0; i < includes_buffer.size(); i++) {
				push_include_cpp(includes_buffer[i]);
			}
			includes_buffer.clear();
			PackedStringArray lines = source_code_buffer.split("\n");
			for (int i = 0; i < lines.size(); i++) {
				if (!lines[i].is_empty()) {
					push_line_cpp(lines[i]);
				}
			}
			source_code_buffer = "";
		}
	};

	GeneratingClass *current = nullptr;

	HashMap<String, String> class_name_cache; // Cache for classes to avoid re-generating them
	Array taken_class_names;
	int unnamed_class_counter = 0;
	String get_class_name(const GDScriptParser::ClassNode *p_class);
	String get_output_file_name(const GDScriptParser::ClassNode *p_class);

	String to_snake_case_no_number_split(const String &p_name);
	String get_include(const GDScriptParser::DataType p_datatype);
	String get_datatype_name(const GDScriptParser::DataType p_datatype);

	void write_header_file();
	void write_cpp_file();
	void write_register_file();

	bool is_class_valid();

	void gen_header_class();
	void gen_cpp_class();

	void gen_array(const GDScriptParser::ArrayNode *p_array);
	void gen_assignment(const GDScriptParser::AssignmentNode *p_assignment);
	void gen_await(const GDScriptParser::AwaitNode *p_await);
	void gen_binary_op(const GDScriptParser::BinaryOpNode *p_binary_op);
	void gen_call(const GDScriptParser::CallNode *p_call);
	void gen_cast(const GDScriptParser::CastNode *p_cast);
	void gen_constant(const GDScriptParser::ConstantNode *p_constant);
	void gen_dictionary(const GDScriptParser::DictionaryNode *p_dictionary);
	void gen_expression(const GDScriptParser::ExpressionNode *p_expression);
	void gen_for(const GDScriptParser::ForNode *p_for);
	void gen_get_node(const GDScriptParser::GetNodeNode *p_get_node);
	void gen_identifier(const GDScriptParser::IdentifierNode *p_identifier);
	void gen_if(const GDScriptParser::IfNode *p_if, bool p_is_elif = false);
	void gen_lambda(const GDScriptParser::LambdaNode *p_lambda);
	void gen_literal(const GDScriptParser::LiteralNode *p_literal);
	void gen_match(const GDScriptParser::MatchNode *p_match);
	void gen_preload(const GDScriptParser::PreloadNode *p_preload);
	void gen_return(const GDScriptParser::ReturnNode *p_return);
	void gen_self(const GDScriptParser::SelfNode *p_self);
	void gen_statement(const GDScriptParser::Node *p_statement);
	void gen_subscript(const GDScriptParser::SubscriptNode *p_subscript);
	void gen_suite(const GDScriptParser::SuiteNode *p_suite);
	void gen_ternary_op(const GDScriptParser::TernaryOpNode *p_ternary_op);
	void gen_type(const GDScriptParser::TypeNode *p_type);
	void gen_type_test(const GDScriptParser::TypeTestNode *p_type);
	void gen_unary_op(const GDScriptParser::UnaryOpNode *p_unary_op);
	void gen_variable(const GDScriptParser::VariableNode *p_variable);
	void gen_while(const GDScriptParser::WhileNode *p_while);

protected:
	static void _bind_methods();

public:
	struct GeneratedClass {
		String class_name;
		String file_path;
		String output_file_name; // shared between header and cpp
		bool is_abstract = false;
	};
	Vector<GeneratedClass *> generated_classes;
	GDScriptCPPGen() {
		DirAccess::remove_absolute(output_folder.path_join("gen"));
	}
	~GDScriptCPPGen() {
		clear();
	}
	void setup_output_folder();
	void generate(const PackedStringArray &p_file_path);
	void clear();
};
