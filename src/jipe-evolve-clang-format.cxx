#include "jipe-evolve-clang-format.hxx"

extern "C" {
#include <fcntl.h>
#include <git2/diff.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
}

#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "clang/Format/Format.h"

#include "../ext/json.hpp"
#include "presets.hxx"

using json = nlohmann::json;

struct userdata {
  ssize_t counter;
};

/**
 * File callback for debugging
 */
extern "C" int
file_cb(
  git_diff_delta const* const delta,
  float const progress,
  void* const payload) {

  struct userdata* const userdata = (struct userdata*)payload;

  using json = nlohmann::json;

  json debug = {{"old_file", delta->old_file.path},
                {"new_file", delta->new_file.path},
                {"nfiles", delta->nfiles},
                {"similarity", delta->similarity},
                {"status", delta->status},
                {"flags", delta->flags}};

  std::cerr << debug << std::endl;

  return 0;
}

/**
 * Callback function for git_diff to handle linewise reports. For
 * each line it accumulates whether a change is being reported in
 * the supplied payload object.
 */
extern "C" int
line_cb(
  git_diff_delta const* const delta,
  git_diff_hunk const* const hunk,
  git_diff_line const* const line,
  void* const payload) {

  struct userdata* const userdata = (struct userdata*)payload;

  userdata->counter += (line->origin == '+' || line->origin == '-');

  return 0;
}

/**
 * Read file contents.
 *
 * @param path
 * @returns file contents
 */
std::string
slurp(std::string const& path) {

  std::ifstream input_stream(path);

  std::string content(
    (std::istreambuf_iterator<char>(input_stream)),
    (std::istreambuf_iterator<char>()));

  return content;
}

/**
 * Measure edit distance between two inputs using libgit2.
 *
 * @param lhs left-hand side string contents
 * @param rhs right-hand side string contents
 *
 * @returns sum of changes
 * @throws When libgit2 returns an error.
 */
ssize_t
gitdifflines(std::string const& lhs, std::string const& rhs) {

  struct userdata userdata = {0};

  git_diff_options options;

  git_diff_init_options(&options, GIT_DIFF_OPTIONS_VERSION);

  options.flags =
    (GIT_DIFF_FORCE_TEXT | GIT_DIFF_MINIMAL |
     GIT_DIFF_SKIP_BINARY_CHECK);

  options.context_lines = 0;

  int fail = git_diff_buffers(
    &lhs[0],
    lhs.size(),
    "/lhs",
    &rhs[0],
    lhs.size(),
    "/rhs",
    &options,
    NULL,
    NULL,
    NULL,
    line_cb,
    &userdata);

  if (fail) {
    throw "git_diff_buffers returned an error";
  }

  return userdata.counter;
}

/**
 * Given a JSON-encoded clang-format configuration and a string of
 * code, format the string using the provided configuration options.
 *
 * @param config JSON-encoded clang-format configuration
 * @param data source code
 *
 * @returns formatted source code
 * @throws when clang::format returns an error
 */
std::string
clangformat(std::string const& config, std::string const& data) {

  auto ours = clang::format::getStyle(
    clang::StringRef(config),
    clang::StringRef("/dev/null"),
    clang::format::DefaultFallbackStyle,
    clang::StringRef(data));

  if (!ours) {

#if DEBUG

    std::cerr << llvm::toString(ours.takeError()) << std::endl;

#endif

    throw "Unable to get style";
  }

#if DEBUG

  std::cerr << clang::format::configurationAsText(ours.get())
            << std::endl;

#endif

  auto formatChanges = clang::format::reformat(
    ours.get(),
    clang::StringRef(data),
    clang::tooling::Range(0, data.size()));

  auto result =
    clang::tooling::applyAllReplacements(data, formatChanges);

  if (!result) {
    throw "Unable to apply changes";
  }

  return result.get();
}

/**
 * Creates string derived from input string with newline added after
 * any byte that is not [[isalnum]] and is not a space. Breaks bytes
 * in multibyte encodings like UTF-8 but that is not a problem here.
 *
 * @param s input string
 *
 * @returns string with newlines added
 */
std::string
add_newlines(std::string const& s) {
  std::string o;

  o.reserve(s.size() * 2);

  std::for_each(s.begin(), s.end(), [&o](char ch) {
    o += ch;
    if (!isalnum(ch) && ch != ' ') {
      o += '\n';
    }
  });

  return o;
}

void
handle_measure(
  json& response, json const& msg, char const* const path) {

  // FIXME: parameter
  auto content = slurp(path);

  auto solution = msg["params"]["solution"];

  auto result = clangformat(solution.dump(), content);

  auto changes =
    gitdifflines(add_newlines(content), add_newlines(result));

#if DEBUG

  std::cerr << solution.dump() << std::endl;
  std::cerr << result << std::endl;
  std::cerr << changes << std::endl;

#endif

  ssize_t count = solution.flatten().size();

  response.erase("error");
  response["result"]["metrics"][path] = {
    {"plusOptions", -(count + changes)},
    {"changeCount", -changes},
  };
}

/**
 * ...
 */
void
handle_template(json& response, json const& msg) {

  auto parsed = json::parse(g_template);

  response.erase("error");
  response["result"] = {{"template", parsed}};
}

/**
 * Check and dispatch incoming JSON-RPC 2.0 messages.
 *
 * @param line message as a string
 */
void
handlemessage(std::string const& line, char const* const path) {

  auto msg = json::parse(line);

  json response = {{"jsonrpc", "2.0"},
                   {"id", msg["id"]},
                   {"error", {{"code", 0}, {"message", ""}}}};

  if (msg["method"].is_null()) {
    return;
  }

  if (msg["method"] == std::string("evolve.measure")) {

    handle_measure(response, msg, path);

  } else if (msg["method"] == std::string("evolve.template")) {

    handle_template(response, msg);

  } else {

    response["error"]["code"] = -32601;
  }

  std::cout << response << std::endl;
}

/**
 * The `evolve` protocol allows us to ask that certain solutions be
 * added to the solution space using the `evolve.populate` request.
 * This could be a solution from a previous run, or one that has been
 * created heuristically, or as in our case, presets that are known
 * to be good starting points for some cases. This function adds the
 * `clang-format` `BasedOnStyle` configurations as seed solutions.
 */
void
populate(size_t& nextId) {

  json populateNotification = {
    {"jsonrpc", "2.0"},
    {"method", "evolve.populate"},
    {"params", {{"solutions", json::array()}}}};

  for (auto i = 0; g_presets[i]; ++i) {

    auto preset = json::parse(g_presets[i]);

    populateNotification["params"]["solutions"].push_back(preset);
  }

  std::cout << populateNotification << std::endl;
}

/**
 * After sending a `jipe.start` message, this reads lines from STDIN,
 * decodes them assuming they are JSON-RPC 2.0 messages, and responds
 * to them as appropriate.
 */
void
rpcloop(char const* const path) {

  size_t nextId = 1;

  json startRequest = {
    {"jsonrpc", "2.0"},
    {"id", nextId++},
    {"method", "jipe.start"},
    {"params",
     {{"implements",
       json::array(
         {"request.evolve.measure", "request.evolve.template"})}}}};

  std::cout << startRequest << std::endl;

  // TODO(bh): wait for response and check that evolve.populate is
  // supported.

  populate(nextId);

  for (std::string line; std::getline(std::cin, line); /**/) {
    handlemessage(line, path);
  }
}

int
main(int argc, char const* const* const argv) {

  std::ios::sync_with_stdio(false);
  std::cin.tie(NULL);

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " example.cpp" << std::endl;
    exit(1);
  }

  rpcloop(argv[1]);

  return 0;
}
