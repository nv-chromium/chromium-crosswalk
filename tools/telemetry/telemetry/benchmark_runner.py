# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parses the command line, discovers the appropriate benchmarks, and runs them.

Handles benchmark configuration, but all the logic for
actually running the benchmark is in Benchmark and PageRunner."""

import hashlib
import inspect
import json
import logging
import os
import sys


# We need to set logging format here to make sure that any other modules
# imported by telemetry doesn't set the logging format before this, which will
# make this a no-op call.
# (See: https://docs.python.org/2/library/logging.html#logging.basicConfig)
logging.basicConfig(
  format=('(%(levelname)s) %(filename)s:%(funcName)s:%(lineno)d '
          '%(asctime)s:%(message)s'))


from telemetry import benchmark
from telemetry.core import discover
from telemetry import decorators
from telemetry.internal.browser import browser_finder
from telemetry.internal.browser import browser_options
from telemetry.internal.util import binary_manager
from telemetry.internal.util import command_line


def PrintBenchmarkList(benchmarks, possible_browser, output_pipe=sys.stdout):
  """ Print benchmarks that are not filtered in the same order of benchmarks in
  the |benchmarks| list.

  Args:
    benchmarks: the list of benchmarks to be printed (in the same order of the
      list).
    possible_browser: the possible_browser instance that's used for checking
      which benchmarks are enabled.
    output_pipe: the stream in which benchmarks are printed on.
  """
  if not benchmarks:
    print >> output_pipe, 'No benchmarks found!'
    return
  b = None  # Need this to stop pylint from complaining undefined variable.
  if any(not issubclass(b, benchmark.Benchmark) for b in benchmarks):
    assert False, '|benchmarks| param contains non benchmark class: %s' % b

  # Align the benchmark names to the longest one.
  format_string = '  %%-%ds %%s' % max(len(b.Name()) for b in benchmarks)
  disabled_benchmarks = []

  print >> output_pipe, 'Available benchmarks %sare:' % (
      'for %s ' %possible_browser.browser_type if possible_browser else '')
  for benchmark_class in benchmarks:
    if possible_browser and not decorators.IsEnabled(benchmark_class,
                                                     possible_browser)[0]:
      disabled_benchmarks.append(benchmark_class)
      continue
    print >> output_pipe, format_string % (
        benchmark_class.Name(), benchmark_class.Description())

  if disabled_benchmarks:
    print >> output_pipe
    print >> output_pipe, (
        'Disabled benchmarks for %s are (force run with -d):' %
        possible_browser.browser_type)
    for benchmark_class in disabled_benchmarks:
      print >> output_pipe, format_string % (
          benchmark_class.Name(), benchmark_class.Description())
  print >> output_pipe, (
      'Pass --browser to list benchmarks for another browser.')
  print >> output_pipe


class ProjectConfig(object):
  """Contains information about the benchmark runtime environment.

  Attributes:
    top_level_dir: A dir that contains benchmark, page test, and/or story
        set dirs and associated artifacts.
    benchmark_dirs: A list of dirs containing benchmarks.
    benchmark_aliases: A dict of name:alias string pairs to be matched against
        exactly during benchmark selection.
    client_config: A path to a ProjectDependencies json file.
  """
  def __init__(self, top_level_dir, benchmark_dirs=None,
               benchmark_aliases=None, client_config=None):
    self._top_level_dir = top_level_dir
    self._benchmark_dirs = benchmark_dirs or []
    self._benchmark_aliases = benchmark_aliases or dict()
    self._client_config = client_config or ''

  @property
  def top_level_dir(self):
    return self._top_level_dir

  @property
  def benchmark_dirs(self):
    return self._benchmark_dirs

  @property
  def benchmark_aliases(self):
    return self._benchmark_aliases

  @property
  def client_config(self):
    return self._client_config

class Help(command_line.OptparseCommand):
  """Display help information about a command"""

  usage = '[command]'

  def Run(self, args):
    if len(args.positional_args) == 1:
      commands = _MatchingCommands(args.positional_args[0])
      if len(commands) == 1:
        command = commands[0]
        parser = command.CreateParser()
        command.AddCommandLineArgs(parser, None)
        parser.print_help()
        return 0

    print >> sys.stderr, ('usage: %s [command] [<options>]' % _ScriptName())
    print >> sys.stderr, 'Available commands are:'
    for command in _Commands():
      print >> sys.stderr, '  %-10s %s' % (
          command.Name(), command.Description())
    print >> sys.stderr, ('"%s help <command>" to see usage information '
                          'for a specific command.' % _ScriptName())
    return 0


class List(command_line.OptparseCommand):
  """Lists the available benchmarks"""

  usage = '[benchmark_name] [<options>]'

  @classmethod
  def CreateParser(cls):
    options = browser_options.BrowserFinderOptions()
    parser = options.CreateParser('%%prog %s %s' % (cls.Name(), cls.usage))
    return parser

  @classmethod
  def AddCommandLineArgs(cls, parser, _):
    parser.add_option('-j', '--json-output-file', type='string')
    parser.add_option('-n', '--num-shards', type='int', default=1)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args, environment):
    if not args.positional_args:
      args.benchmarks = _Benchmarks(environment)
    elif len(args.positional_args) == 1:
      args.benchmarks = _MatchBenchmarkName(args.positional_args[0],
                                            environment, exact_matches=False)
    else:
      parser.error('Must provide at most one benchmark name.')

  def Run(self, args):
    possible_browser = browser_finder.FindBrowser(args)
    if args.browser_type in (
        'exact', 'release', 'release_x64', 'debug', 'debug_x64', 'canary'):
      args.browser_type = 'reference'
      possible_reference_browser = browser_finder.FindBrowser(args)
    else:
      possible_reference_browser = None
    if args.json_output_file:
      with open(args.json_output_file, 'w') as f:
        f.write(_GetJsonBenchmarkList(possible_browser,
                                      possible_reference_browser,
                                      args.benchmarks, args.num_shards))
    else:
      PrintBenchmarkList(args.benchmarks, possible_browser)
    return 0


class Run(command_line.OptparseCommand):
  """Run one or more benchmarks (default)"""

  usage = 'benchmark_name [page_set] [<options>]'

  @classmethod
  def CreateParser(cls):
    options = browser_options.BrowserFinderOptions()
    parser = options.CreateParser('%%prog %s %s' % (cls.Name(), cls.usage))
    return parser

  @classmethod
  def AddCommandLineArgs(cls, parser, environment):
    benchmark.AddCommandLineArgs(parser)

    # Allow benchmarks to add their own command line options.
    matching_benchmarks = []
    for arg in sys.argv[1:]:
      matching_benchmarks += _MatchBenchmarkName(arg, environment)

    if matching_benchmarks:
      # TODO(dtu): After move to argparse, add command-line args for all
      # benchmarks to subparser. Using subparsers will avoid duplicate
      # arguments.
      matching_benchmark = matching_benchmarks.pop()
      matching_benchmark.AddCommandLineArgs(parser)
      # The benchmark's options override the defaults!
      matching_benchmark.SetArgumentDefaults(parser)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args, environment):
    all_benchmarks = _Benchmarks(environment)
    if not args.positional_args:
      possible_browser = (
          browser_finder.FindBrowser(args) if args.browser_type else None)
      PrintBenchmarkList(all_benchmarks, possible_browser)
      sys.exit(-1)

    input_benchmark_name = args.positional_args[0]
    matching_benchmarks = _MatchBenchmarkName(input_benchmark_name, environment)
    if not matching_benchmarks:
      print >> sys.stderr, 'No benchmark named "%s".' % input_benchmark_name
      print >> sys.stderr
      most_likely_matched_benchmarks = command_line.GetMostLikelyMatchedObject(
          all_benchmarks, input_benchmark_name, lambda x: x.Name())
      if most_likely_matched_benchmarks:
        print >> sys.stderr, 'Do you mean any of those benchmarks below?'
        PrintBenchmarkList(most_likely_matched_benchmarks, None, sys.stderr)
      sys.exit(-1)

    if len(matching_benchmarks) > 1:
      print >> sys.stderr, ('Multiple benchmarks named "%s".' %
                            input_benchmark_name)
      print >> sys.stderr, 'Did you mean one of these?'
      print >> sys.stderr
      PrintBenchmarkList(matching_benchmarks, None, sys.stderr)
      sys.exit(-1)

    benchmark_class = matching_benchmarks.pop()
    if len(args.positional_args) > 1:
      parser.error('Too many arguments.')

    assert issubclass(benchmark_class, benchmark.Benchmark), (
        'Trying to run a non-Benchmark?!')

    benchmark.ProcessCommandLineArgs(parser, args)
    benchmark_class.ProcessCommandLineArgs(parser, args)

    cls._benchmark = benchmark_class

  def Run(self, args):
    return min(255, self._benchmark().Run(args))


def _ScriptName():
  return os.path.basename(sys.argv[0])


def _Commands():
  """Generates a list of all classes in this file that subclass Command."""
  for _, cls in inspect.getmembers(sys.modules[__name__]):
    if not inspect.isclass(cls):
      continue
    if not issubclass(cls, command_line.Command):
      continue
    yield cls

def _MatchingCommands(string):
  return [command for command in _Commands()
         if command.Name().startswith(string)]

@decorators.Cache
def _Benchmarks(environment):
  benchmarks = []
  for search_dir in environment.benchmark_dirs:
    benchmarks += discover.DiscoverClasses(search_dir,
                                           environment.top_level_dir,
                                           benchmark.Benchmark,
                                           index_by_class_name=True).values()
  return benchmarks

def _MatchBenchmarkName(input_benchmark_name, environment, exact_matches=True):
  def _Matches(input_string, search_string):
    if search_string.startswith(input_string):
      return True
    for part in search_string.split('.'):
      if part.startswith(input_string):
        return True
    return False

  # Exact matching.
  if exact_matches:
    # Don't add aliases to search dict, only allow exact matching for them.
    if input_benchmark_name in environment.benchmark_aliases:
      exact_match = environment.benchmark_aliases[input_benchmark_name]
    else:
      exact_match = input_benchmark_name

    for benchmark_class in _Benchmarks(environment):
      if exact_match == benchmark_class.Name():
        return [benchmark_class]
    return []

  # Fuzzy matching.
  return [benchmark_class for benchmark_class in _Benchmarks(environment)
          if _Matches(input_benchmark_name, benchmark_class.Name())]


def GetBenchmarkByName(name, environment):
  matched = _MatchBenchmarkName(name, environment, exact_matches=True)
  # With exact_matches, len(matched) is either 0 or 1.
  if len(matched) == 0:
    return None
  return matched[0]


def _GetJsonBenchmarkList(possible_browser, possible_reference_browser,
                          benchmark_classes, num_shards):
  """Returns a list of all enabled benchmarks in a JSON format expected by
  buildbots.

  JSON format (see build/android/pylib/perf/benchmark_runner.py):
  { "version": <int>,
    "steps": {
      <string>: {
        "device_affinity": <int>,
        "cmd": <string>,
        "perf_dashboard_id": <string>,
      },
      ...
    }
  }
  """
  output = {
    'version': 1,
    'steps': {
    }
  }
  for benchmark_class in benchmark_classes:
    if not issubclass(benchmark_class, benchmark.Benchmark):
      continue
    enabled, _ = decorators.IsEnabled(benchmark_class, possible_browser)
    if not enabled:
      continue

    base_name = benchmark_class.Name()
    base_cmd = [sys.executable, os.path.realpath(sys.argv[0]),
                '-v', '--output-format=chartjson', '--upload-results',
                base_name]
    perf_dashboard_id = base_name
    # TODO(fmeawad): Currently we set the device affinity to a stable hash of
    # the benchmark name. This somewhat evenly distributes benchmarks among the
    # requested number of shards. However, it is far from optimal in terms of
    # cycle time.  We should add a benchmark size decorator (e.g. small, medium,
    # large) and let that inform sharding.

    # Based on the current timings, we shift the result of the hash function to
    # achieve better load balancing. Those shift values are to be revised when
    # necessary. (See tools/build/scripts/tools/perf/chrome-perf-step-timings.py
    # for more details)
    hash_shift = {
      2 : 47,
      5 : 56,
      8 : 50
    }
    shift = hash_shift.get(num_shards, 0)
    base_name_hash = hashlib.sha1(base_name).hexdigest()
    device_affinity = (int(base_name_hash, 16) >> shift) % num_shards

    output['steps'][base_name] = {
      'cmd': ' '.join(base_cmd + [
            '--browser=%s' % possible_browser.browser_type]),
      'device_affinity': device_affinity,
      'perf_dashboard_id': perf_dashboard_id,
    }
    if possible_reference_browser:
      enabled, _ = decorators.IsEnabled(
          benchmark_class, possible_reference_browser)
      if enabled:
        output['steps'][base_name + '.reference'] = {
          'cmd': ' '.join(base_cmd + [
                '--browser=reference', '--output-trace-tag=_ref']),
          'device_affinity': device_affinity,
          'perf_dashboard_id': perf_dashboard_id,
        }

  return json.dumps(output, indent=2, sort_keys=True)


def main(environment):
  # Get the command name from the command line.
  if len(sys.argv) > 1 and sys.argv[1] == '--help':
    sys.argv[1] = 'help'

  command_name = 'run'
  for arg in sys.argv[1:]:
    if not arg.startswith('-'):
      command_name = arg
      break

  # TODO(eakuefner): Remove this hack after we port to argparse.
  if command_name == 'help' and len(sys.argv) > 2 and sys.argv[2] == 'run':
    command_name = 'run'
    sys.argv[2] = '--help'

  # Validate and interpret the command name.
  commands = _MatchingCommands(command_name)
  if len(commands) > 1:
    print >> sys.stderr, ('"%s" is not a %s command. Did you mean one of these?'
                          % (command_name, _ScriptName()))
    for command in commands:
      print >> sys.stderr, '  %-10s %s' % (
          command.Name(), command.Description())
    return 1
  if commands:
    command = commands[0]
  else:
    command = Run

  binary_manager.InitDependencyManager(environment.client_config)

  # Parse and run the command.
  parser = command.CreateParser()
  command.AddCommandLineArgs(parser, environment)
  options, args = parser.parse_args()
  if commands:
    args = args[1:]
  options.positional_args = args
  command.ProcessCommandLineArgs(parser, options, environment)
  return command().Run(options)
