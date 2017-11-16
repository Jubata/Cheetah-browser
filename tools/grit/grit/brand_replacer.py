#!/usr/bin/env python
# Copyright (c) 2017 Jubata. All rights reserved.

'''Replace brand string in translations and messages
'''

from grit import lazy_re

old_products = '(Chromium|Chrome|Chromu|Chroma|Chromom|Chromovimi|Chromovim|Chromovi|Chromove|Chromov)'
old_company = 'Google'

class BrandReplacer:
    '''Replaces brands in strings
    '''

    def BuildReVariants(self, old, new):
        rexp = lazy_re.compile(r'\b%s\b' % old)
        self.templates.append((rexp, new))

    def __init__(self, new_product, new_company):
        self.templates = []

        self.BuildReVariants(old_products, new_product)
        self.BuildReVariants(old_company, new_company)

    def Replace(self, text):
        for (rexp, subs) in self.templates:
            text = rexp.sub(subs, text)

        return text

if __name__ == '__main__':
    r = BrandReplacer('Product', 'Company')
    r.Replace('chrome Chrome CHROME google Google GOOGLE')


