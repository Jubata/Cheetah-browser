#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The 'grit auto_translate' tool.
"""

import getopt
import os

from grit import grd_reader
from grit import util
from grit.tool import interface
from grit.node import message
import requests

baseUrl = "https://www.googleapis.com/language/translate/v2?key=%s"

def GoogleTranslate(text, source='en', target='zh-CN'):
  if target == u'en-GB': return text
  if target == u'es-419': target = 'es'
  GOOGLE_TRANSLATION_API = os.environ['GOOGLE_TRANSLATION_API']
  data = {"source":source, "target":target, "q":text}
  response = requests.post(baseUrl % GOOGLE_TRANSLATION_API, json = data )
  if response.status_code != 200:
    raise Exception(response.content)
  data = response.json()
  return [el['translatedText'].encode('utf-8') for el in data['data']['translations']]



class Translate(interface.Tool):
  """Translate messages with missing translation in the .grd input file.
Overwrites existing xtb files with new translations.

Usage: grit auto_translate [-i|-h] [-l LIMITFILE]

Other options:

  -D NAME[=VAL]     Specify a C-preprocessor-like define NAME with optional
                    value VAL (defaults to 1) which will be used to control
                    conditional inclusion of resources.

  -E NAME=VALUE     Set environment variable NAME to VALUE (within grit).

"""

  def __init__(self, defines=None):
    super(Translate, self).__init__()
    self.defines = defines or {}

  def ShortDescription(self):
    return 'Translates messages with missing translation.'

  def Run(self, opts, args):
    self.SetOptions(opts)

    own_opts, args = getopt.getopt(args, 'D:h')
    for key, val in own_opts:
      if key == '-D':
        name, val = util.ParseDefine(val)
        self.defines[name] = val
      elif key == '-E':
        (env_name, env_value) = val.split('=', 1)
        os.environ[env_name] = env_value

    res_tree = grd_reader.Parse(opts.input, debug=opts.extra_verbose)
    res_tree.SetOutputLanguage('en')
    res_tree.SetDefines(self.defines)
    res_tree.RunGatherers()

    self.Process(res_tree)
    print "done"

  def Process(self, res_tree):
    """Writes missing translations to xtb files. overwrites existing files

    Args:
      res_tree: base.Node()
    """
    xtbnodes = [xtb for xtb in res_tree.ActiveDescendants()
      if(xtb.name == 'file' and xtb.parent and xtb.parent.name == 'translations')]
    lang2xtb = {}
    for xtb in xtbnodes:
      lang2xtb[xtb.GetLang()] = xtb

      for item in res_tree.ActiveDescendants():
        with item:
          if IsMessageNode(item):
            item.Translate(xtb.GetLang())

    lang2cliques = {}
    for (clique, lang) in res_tree.UberClique().AllMissingTranslations():
      if not lang in lang2cliques:
        lang2cliques[lang] = []
      lang2cliques[lang].append(clique)

    for (lang, cliques) in lang2cliques.iteritems():
      texts = []
      for clique in cliques:
        texts.append( clique.GetMessage().GetPresentableContent() )

      translates = GoogleTranslate(texts, 'en', lang)

      addendum = []

      for (clique, text) in zip(cliques, translates):
        line = "<translation id=\"%s\">%s</translation>\n" % (clique.GetId(), text)
        addendum.append(line)

      xtb = lang2xtb[lang]
      filepath = xtb.ToRealPath( xtb.GetInputPath() )
      file = open(filepath, 'r+')
      lines = file.readlines()
      lines = lines[:-1] + addendum + lines[-1:]
      file.seek(0)
      file.writelines(lines)
      file.truncate()
      file.close()

def IsMessageNode(node):
  return (isinstance(node, message.MessageNode))
