# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander
from collections import defaultdict
from make_runtime_features_utilities import origin_trials


class FeaturePolicyFeatureWriter(json5_generator.Writer):
    file_basename = 'policy_helper'

    def __init__(self, json5_file_path, output_dir):
        super(FeaturePolicyFeatureWriter, self).__init__(
            json5_file_path, output_dir)
        runtime_features = []
        feature_policy_features = []
        # Note: there can be feature with same 'name' attribute in
        # document_policy_features and in feature_policy_features.
        # They are supposed to have the same 'depends_on' attribute.
        # However, their feature_policy_name and document_policy_name
        # might be different.
        document_policy_features = []

        for feature in self.json5_file.name_dictionaries:
            if feature['feature_policy_name']:
                feature_policy_features.append(feature)
            elif feature['document_policy_name']:
                document_policy_features.append(feature)
            else:
                runtime_features.append(feature)

        origin_trials_set = origin_trials(runtime_features)
        fp_origin_trial_dependency_map = defaultdict(list)
        dp_origin_trial_dependency_map = defaultdict(list)
        runtime_to_feature_policy_map = defaultdict(list)
        runtime_to_document_policy_map = defaultdict(list)
        for feature in feature_policy_features + document_policy_features:
            for dependency in feature['depends_on']:
                if str(dependency) in origin_trials_set:
                    if feature['feature_policy_name']:
                        fp_origin_trial_dependency_map[feature['name']].append(
                            dependency)
                    else:
                        dp_origin_trial_dependency_map[feature['name']].append(
                            dependency)
                else:
                    if feature['feature_policy_name']:
                        runtime_to_feature_policy_map[dependency].append(
                            feature['name'])
                    else:
                        runtime_to_document_policy_map[dependency].append(
                            feature['name'])

        self._outputs = {
            self.file_basename + '.cc':
                template_expander.use_jinja('templates/' +
                    self.file_basename + '.cc.tmpl')(lambda: {
                        'header_guard': self.make_header_guard(
                            self._relative_output_dir +
                            self.file_basename + '.h'),
                        'input_files': self._input_files,
                        'feature_policy_features': feature_policy_features,
                        'document_policy_features': document_policy_features,
                        'fp_origin_trial_dependency_map':
                        fp_origin_trial_dependency_map,
                        'dp_origin_trial_dependency_map':
                        dp_origin_trial_dependency_map,
                        'runtime_to_feature_policy_map':
                        runtime_to_feature_policy_map,
                        'runtime_to_document_policy_map':
                        runtime_to_document_policy_map
                    }),
        }


if __name__ == '__main__':
    json5_generator.Maker(FeaturePolicyFeatureWriter).main()
