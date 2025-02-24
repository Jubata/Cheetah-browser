# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for dealing with determining --tool-prefix."""

import abc
import distutils.spawn
import logging
import os

_STATUS_DETECTED = 1
_STATUS_VERIFIED = 2

SRC_ROOT = os.environ.get('CHECKOUT_SOURCE_ROOT',
    os.path.abspath(os.path.join(os.path.dirname(__file__),
                                 os.pardir, os.pardir, os.pardir)))

_SAMPLE_TOOL_SUFFIX = 'readelf'


class _PathFinder(object):
  def __init__(self, name, value):
    self._status = _STATUS_DETECTED if value is not None else 0
    self._name = name
    self._value = value

  @abc.abstractmethod
  def Detect(self):
    pass

  @abc.abstractmethod
  def Verify(self):
    pass

  def Tentative(self):
    if self._status < _STATUS_DETECTED:
      self._value = self.Detect()
      logging.debug('Detected --%s=%s', self._name, self._value)
      self._status = _STATUS_DETECTED
    return self._value

  def Finalized(self):
    if self._status < _STATUS_VERIFIED:
      self.Tentative()
      self.Verify()
      logging.info('Using --%s=%s', self._name, self._value)
      self._status = _STATUS_VERIFIED
    return self._value


class OutputDirectoryFinder(_PathFinder):
  def __init__(self, value=None, any_path_within_output_directory=None):
    super(OutputDirectoryFinder, self).__init__(
        name='output-directory', value=value)
    self._any_path_within_output_directory = any_path_within_output_directory

  def Detect(self):
    # Try and find build.ninja.
    abs_path = os.path.abspath(self._any_path_within_output_directory)
    while True:
      if os.path.exists(os.path.join(abs_path, 'build.ninja')):
        return os.path.relpath(abs_path)
      parent_dir = os.path.dirname(abs_path)
      if parent_dir == abs_path:
        break
      abs_path = abs_path = parent_dir

    # See if CWD=output directory.
    if os.path.exists('build.ninja'):
      return '.'
    return None

  def Verify(self):
    if not self._value or not os.path.isdir(self._value):
      raise Exception('Bad --%s. Path not found: %s' %
                      (self._name, self._value))


class ToolPrefixFinder(_PathFinder):
  def __init__(self, value=None, output_directory_finder=None,
               linker_name=None):
    super(ToolPrefixFinder, self).__init__(
        name='tool-prefix', value=value)
    self._output_directory_finder = output_directory_finder
    self._linker_name = linker_name;

  def Detect(self):
    output_directory = self._output_directory_finder.Tentative()
    if output_directory:
      ret = None
      if self._linker_name == 'lld':
        ret = os.path.join(SRC_ROOT, 'third_party', 'llvm-build',
                           'Release+Asserts', 'bin', 'llvm-')
      else:
        # Auto-detect from build_vars.txt
        build_vars_path = os.path.join(output_directory, 'build_vars.txt')
        if os.path.exists(build_vars_path):
          with open(build_vars_path) as f:
            build_vars = dict(l.rstrip().split('=', 1) for l in f if '=' in l)
          tool_prefix = build_vars['android_tool_prefix']
          ret = os.path.normpath(os.path.join(output_directory, tool_prefix))
          # Maintain a trailing '/' if needed.
          if tool_prefix.endswith(os.path.sep):
            ret += os.path.sep
      if ret:
        # Check for output directories that have a stale build_vars.txt.
        if os.path.isfile(ret + _SAMPLE_TOOL_SUFFIX):
          return ret
        else:
          logging.warn('Invalid default tool-prefix: %s', ret)
          # TODO(huangs): For LLD, print more instruction on how to download
          # or build the required tools.
    from_path = distutils.spawn.find_executable(_SAMPLE_TOOL_SUFFIX)
    if from_path:
      return from_path[:-7]
    return None

  def Verify(self):
    if os.path.sep not in self._value:
      full_path = distutils.spawn.find_executable(
          self._value + _SAMPLE_TOOL_SUFFIX)
    else:
      full_path = self._value + _SAMPLE_TOOL_SUFFIX
    if not full_path or not os.path.isfile(full_path):
      raise Exception('Bad --%s. Path not found: %s' % (self._name, full_path))


def FromSrcRootRelative(path):
  ret = os.path.relpath(os.path.join(SRC_ROOT, path))
  # Need to maintain a trailing /.
  if path.endswith(os.path.sep):
    ret += os.path.sep
  return ret


def ToSrcRootRelative(path):
  ret = os.path.relpath(path, SRC_ROOT)
  # Need to maintain a trailing /.
  if path.endswith(os.path.sep):
    ret += os.path.sep
  return ret


def GetCppFiltPath(tool_prefix):
  if tool_prefix[-5:] == 'llvm-':
    return tool_prefix + 'cxxfilt'
  return tool_prefix + 'c++filt'


def GetNmPath(tool_prefix):
  return tool_prefix + 'nm'


def GetObjDumpPath(tool_prefix):
  return tool_prefix + 'objdump'


def GetReadElfPath(tool_prefix):
  # Work-around for llvm-readobj bug where 'File: ...' info is not printed:
  # https://bugs.llvm.org/show_bug.cgi?id=35351
  if tool_prefix[-5:] == 'llvm-':
    return 'readelf'
  return tool_prefix + 'readelf'
