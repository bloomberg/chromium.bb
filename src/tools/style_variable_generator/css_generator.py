# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from base_generator import Color, Modes, BaseGenerator, VariableType
import collections


class CSSStyleGenerator(BaseGenerator):
    '''Generator for CSS Variables'''

    @staticmethod
    def GetName():
        return 'CSS'

    def Render(self):
        self.Validate()
        return self.ApplyTemplate(self, 'css_generator.tmpl',
                                  self.GetParameters())

    def GetParameters(self):
        def BuildColorsForMode(mode, resolve_missing=False):
            '''Builds a name to Color dictionary for |mode|.
            If |resolve_missing| is true, colors that aren't specified in |mode|
            will be resolved to their default mode value.'''
            colors = collections.OrderedDict()
            for name, mode_values in self.model[VariableType.COLOR].items():
                if resolve_missing:
                    colors[name] = self.model[VariableType.COLOR].Resolve(
                        name, mode)
                else:
                    if mode in mode_values:
                        colors[name] = mode_values[mode]
            return colors

        if self.generate_single_mode:
            return {
                'light_colors':
                BuildColorsForMode(self.generate_single_mode,
                                   resolve_missing=True)
            }

        return {
            'light_colors': BuildColorsForMode(Modes.LIGHT),
            'dark_colors': BuildColorsForMode(Modes.DARK),
        }

    def GetFilters(self):
        return {
            'to_css_var_name': self._ToCSSVarName,
            'css_color': self._CssColor,
            'css_color_rgb': self._CssColorRGB,
        }

    def GetGlobals(self):
        return {
            'css_color_from_rgb_var': self._CssColorFromRGBVar,
            'in_files': self.in_file_to_context.keys(),
        }

    def GetCSSVarNames(self):
        '''Returns generated CSS variable names (excluding the rgb versions)'''
        names = set()
        for name in self.model[VariableType.COLOR].keys():
            names.add(self._ToCSSVarName(name))

        return names

    def _GetCSSVarPrefix(self, model_name):
        prefix = self.context_map[model_name].get('prefix')
        return prefix + '-' if prefix else ''

    def _ToCSSVarName(self, model_name):
        return '--%s%s' % (self._GetCSSVarPrefix(model_name),
                           model_name.replace('_', '-'))

    def _CssColor(self, c):
        '''Returns the CSS color representation of |c|'''
        assert (isinstance(c, Color))
        if c.var:
            return 'var(%s)' % self._ToCSSVarName(c.var)

        if c.rgb_var:
            if c.a != 1:
                return 'rgba(var(%s-rgb), %g)' % (self._ToCSSVarName(
                    c.RGBVarToVar()), c.a)
            else:
                return 'rgb(var(%s-rgb))' % self._ToCSSVarName(c.RGBVarToVar())

        if c.a != 1:
            return 'rgba(%d, %d, %d, %g)' % (c.r, c.g, c.b, c.a)
        else:
            return 'rgb(%d, %d, %d)' % (c.r, c.g, c.b)

    def _CssColorRGB(self, c):
        '''Returns the CSS rgb representation of |c|'''
        if c.var:
            return 'var(%s-rgb)' % self._ToCSSVarName(c.var)

        if c.rgb_var:
            return 'var(%s-rgb)' % self._ToCSSVarName(c.RGBVarToVar())

        return '%d, %d, %d' % (c.r, c.g, c.b)

    def _CssColorFromRGBVar(self, model_name, alpha):
        '''Returns the CSS color representation given a color name and alpha'''
        if alpha != 1:
            return 'rgba(var(%s-rgb), %g)' % (self._ToCSSVarName(model_name),
                                              alpha)
        else:
            return 'rgb(var(%s-rgb))' % self._ToCSSVarName(model_name)
