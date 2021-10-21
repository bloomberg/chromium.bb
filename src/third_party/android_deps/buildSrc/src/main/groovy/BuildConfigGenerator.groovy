// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import groovy.json.JsonOutput
import groovy.text.SimpleTemplateEngine
import groovy.text.Template
import groovy.transform.SourceURI
import org.gradle.api.DefaultTask
import org.gradle.api.Project
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.TaskAction

import java.nio.file.Path
import java.nio.file.Paths
import java.util.concurrent.Executors
import java.util.concurrent.ExecutorService
import java.util.concurrent.Future
import java.util.regex.Pattern
import java.util.regex.Matcher

/**
 * Task to download dependencies specified in {@link ChromiumPlugin} and configure the Chromium build to integrate them.
 * Used by declaring a new task in a {@code build.gradle} file:
 * <pre>
 * task myTaskName(type: BuildConfigGenerator) {
 *   repositoryPath 'build_files_and_repository_location/'
 * }
 * </pre>
 */
class BuildConfigGenerator extends DefaultTask {

    private static final String BUILD_GN_TOKEN_START = '# === Generated Code Start ==='
    private static final String BUILD_GN_TOKEN_END = '# === Generated Code End ==='
    private static final Pattern BUILD_GN_GEN_PATTERN = Pattern.compile(
            "${BUILD_GN_TOKEN_START}(.*)${BUILD_GN_TOKEN_END}", Pattern.DOTALL)
    private static final String GEN_REMINDER =
            '# This is generated, do not edit. Update BuildConfigGenerator.groovy instead.\n'
    private static final String DEPS_TOKEN_START = '# === ANDROID_DEPS Generated Code Start ==='
    private static final String DEPS_TOKEN_END = '# === ANDROID_DEPS Generated Code End ==='
    private static final Pattern DEPS_GEN_PATTERN = Pattern.compile(
            "${DEPS_TOKEN_START}(.*)${DEPS_TOKEN_END}", Pattern.DOTALL)
    private static final String DOWNLOAD_DIRECTORY_NAME = 'libs'
    // The 3pp bot now adds an epoch to the version tag, this needs to be kept in sync with 3pp epoch at:
    /* groovylint-disable-next-line LineLength */
    // https://source.chromium.org/chromium/infra/infra/+/master:recipes/recipe_modules/support_3pp/resolved_spec.py?q=symbol:PACKAGE_EPOCH&ss=chromium
    private static final String THREEPP_EPOCH = '2'

    // Some libraries are hosted in Chromium's //third_party directory. This is a mapping between
    // them so they can be used instead of android_deps pulling in its own copy.
    static final Map<String, String> EXISTING_LIBS = [
        com_ibm_icu_icu4j: '//third_party/icu4j:icu4j_java',
        com_almworks_sqlite4java_sqlite4java: '//third_party/sqlite4java:sqlite4java_java',
        com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework:
            '//third_party/accessibility_test_framework:accessibility_test_framework_java',
        junit_junit: '//third_party/junit:junit',
        org_bouncycastle_bcprov_jdk15on: '//third_party/bouncycastle:bouncycastle_java',
        org_hamcrest_hamcrest_core: '//third_party/hamcrest:hamcrest_core_java',
        org_hamcrest_hamcrest_integration: '//third_party/hamcrest:hamcrest_integration_java',
        org_hamcrest_hamcrest_library: '//third_party/hamcrest:hamcrest_library_java',
    ]

    /**
     * Prefixes of androidx dependencies which are allowed to use non-SNAPSHOT
     * versions. These are the legacy androidx targets that are no longer being
     * released regularly (thus are not part of the snapshots) but are still
     * required by chromium.
     *
     * If an assert fails pointing at a dep which *does* exist in the androidx
     * snapshot then adding it here will only silence the sanity check rather
     * than fix the underlying issue (which is we should always use the snapshot
     * versions of androidx deps when possible). A better solution could be to
     * add it in //third_party/androidx/build.gradle.template
     */
    static final Set<String> ALLOWED_ANDROIDX_NON_SNAPSHOT_DEPS_PREFIXES = [
      'androidx_constraintlayout',
      'androidx_documentfile',
      'androidx_legacy',
      'androidx_multidex_multidex',
      'androidx_print',
      'androidx_test',
    ]

    // Prefixes of autorolled libraries in //third_party/android_deps_autorolled.
    static final List<String> AUTOROLLED_LIB_PREFIXES = []

    static final String AUTOROLLED_REPO_PATH = 'third_party/android_deps_autorolled'

    static final String COPYRIGHT_HEADER = '''\
        # Copyright 2021 The Chromium Authors. All rights reserved.
        # Use of this source code is governed by a BSD-style license that can be
        # found in the LICENSE file.
    '''.stripIndent()

    /**
     * Directory where the artifacts will be downloaded and where files will be generated.
     * Note: this path is specified as relative to the chromium source root, and must be normalised
     * to an absolute path before being used, as Groovy would base relative path where the script
     * is being executed.
     */
    @Input
    String repositoryPath

    /** Relative path to the Chromium source root from the build.gradle file. */
    @Input
    String chromiumSourceRoot

    /** Name of the cipd root package. */
    @Input
    String cipdBucket

    /** Skips license file import. */
    @Input
    boolean skipLicenses

    /** Array with visibility for targets which are not listed in build.gradle */
    @Input
    String[] internalTargetVisibility

    /** Whether to ignore DEPS file. */
    @Input
    boolean ignoreDEPS

    /** The URI of the file BuildConfigGenerator.groovy */
    @Input
    @SourceURI
    URI sourceUri

    static String translateTargetName(String targetName) {
        if (isPlayServicesTarget(targetName)) {
            return targetName.replaceFirst('com_', '').replaceFirst('android_gms_', '')
        }
        return targetName
    }

    static boolean isPlayServicesTarget(String dependencyId) {
        // Firebase has historically been treated as a part of play services, so it counts here for backwards
        // compatibility. Datatransport is new as of 2019 and is used by many play services libraries.
        return Pattern.matches('.*google.*(play_services|firebase|datatransport).*', dependencyId)
    }

    static String makeOwners() {
        // Make it easier to upgrade existing dependencies without full third_party review.
        return 'file://third_party/android_deps/OWNERS\n'
    }

    static String makeReadme(ChromiumDepGraph.DependencyDescription dependency) {
        List<String> licenseStrings = []
        for (ChromiumDepGraph.LicenseSpec license : dependency.licenses) {
            // Replace license names with ones that are whitelisted, see third_party/PRESUBMIT.py
            switch (license.name) {
                case 'The Apache License, Version 2.0':
                case 'The Apache Software License, Version 2.0':
                    licenseStrings.add('Apache Version 2.0')
                    break
                case 'GNU General Public License, version 2, with the Classpath Exception':
                    licenseStrings.add('GPL v2 with the classpath exception')
                    break
                default:
                    licenseStrings.add(license.name)
            }
        }
        String licenseString = String.join(', ', licenseStrings)

        boolean securityCritical = dependency.supportsAndroid && dependency.isShipped
        String licenseFile = dependency.isShipped ? 'LICENSE' : 'NOT_SHIPPED'

        return """\
            Name: ${dependency.displayName}
            Short Name: ${dependency.name}
            URL: ${dependency.url}
            Version: ${dependency.version}
            License: ${licenseString}
            License File: ${licenseFile}
            Security Critical: ${securityCritical ? 'yes' : 'no'}
            ${dependency.licenseAndroidCompatible ? 'License Android Compatible: yes' : ''}
            Description:
            ${dependency.description}

            Local Modifications:
            No modifications.
            """.stripIndent()
    }

    static String makeCipdYaml(ChromiumDepGraph.DependencyDescription dependency, String cipdBucket, String repoPath) {
        String cipdVersion = "${THREEPP_EPOCH}@${dependency.version}.${dependency.cipdSuffix}"
        String cipdPath = "${cipdBucket}/${repoPath}"
        // CIPD does not allow uppercase in names.
        cipdPath += "/${DOWNLOAD_DIRECTORY_NAME}/$dependency.directoryName"

        // NOTE: The fetch_all.py script relies on the format of this file! See fetch_all.py:GetCipdPackageInfo().
        // NOTE: Keep the copyright year 2018 until this generated code is updated, avoiding annual churn of all
        //       cipd.yaml files.
        return """\
            # Copyright 2018 The Chromium Authors. All rights reserved.
            # Use of this source code is governed by a BSD-style license that can be
            # found in the LICENSE file.

            # To create CIPD package run the following command.
            # cipd create --pkg-def cipd.yaml -tag version:${cipdVersion}
            package: ${cipdPath}
            description: "${dependency.displayName}"
            data:
            - file: ${dependency.fileName}
            """.stripIndent()
    }

    static void validateLicenses(ChromiumDepGraph.DependencyDescription dependency) {
        if (dependency.licenses.empty) {
            throw new RuntimeException("Missing license for ${dependency.id}.")
        }

        for (ChromiumDepGraph.LicenseSpec license : dependency.licenses) {
            if (!license.path?.trim() && !license.url?.trim()) {
                throw new RuntimeException("Missing license for ${dependency.id}. License Name was: ${license.name}")
            }
        }
    }

    static void downloadLicenses(ChromiumDepGraph.DependencyDescription dependency,
                                 String normalisedRepoPath,
                                 ExecutorService downloadExecutor,
                                 List<Future> downloadTasks) {
        String depDir = "$normalisedRepoPath/${computeDepDir(dependency)}"
        for (int i = 0; i < dependency.licenses.size(); ++i) {
            ChromiumDepGraph.LicenseSpec license = dependency.licenses[i]
            if (!license.path?.trim() && license.url?.trim()) {
                String destFileSuffix = (dependency.licenses.size() > 1) ? "${i + 1}.tmp" : ''
                File destFile = new File("${depDir}/LICENSE${destFileSuffix}")
                downloadTasks.add(downloadExecutor.submit {
                    downloadFile(dependency.id, license.url, destFile)
                    if (destFile.text.contains('<html')) {
                        throw new RuntimeException('Found HTML in LICENSE file. Please add an '
                                + "override to ChromiumDepGraph.groovy for ${dependency.id}.")
                    }
                })
            }
        }
                                 }

    static void mergeLicenses(ChromiumDepGraph.DependencyDescription dependency, String normalisedRepoPath) {
        String depDir = computeDepDir(dependency)
        File outFile = new File("${normalisedRepoPath}/${depDir}/LICENSE")

        if (dependency.licenses.size() == 1) {
            String licensePath0 = dependency.licenses.get(0).path?.trim()
            if (licensePath0) {
                outFile.write(new File("${normalisedRepoPath}/${licensePath0}").text)
            }
            return
        }

        outFile.write('Third-Party Software Licenses\n')
        for (int i = 0; i < dependency.licenses.size(); ++i) {
            ChromiumDepGraph.LicenseSpec licenseSpec = dependency.licenses[i]
            outFile.append("\n${i + 1}. ${licenseSpec.name}\n\n")
            String licensePath = licenseSpec.path ? licenseSpec.path.trim() : "${depDir}/LICENSE${i + 1}.tmp"
            outFile.append(new File("${normalisedRepoPath}/${licensePath}").text)
        }
    }

    static String make3ppPb(ChromiumDepGraph.DependencyDescription dependency, String cipdBucket, String repoPath) {
        String pkgPrefix = "${cipdBucket}/${repoPath}/${DOWNLOAD_DIRECTORY_NAME}"

        return COPYRIGHT_HEADER + '\n' + GEN_REMINDER + """
            create {
              source {
                script { name: "fetch.py" }
                patch_version: "${dependency.cipdSuffix}"
              }
            }

            upload {
              pkg_prefix: "${pkgPrefix}"
              universal: true
            }
            """.stripIndent()
    }

    static String make3ppFetch(Template fetchTemplate, ChromiumDepGraph.DependencyDescription dependency) {
        Map bindMap = [
            copyrightHeader: COPYRIGHT_HEADER,
            dependency: dependency,
        ]
        return fetchTemplate.make(bindMap).toString()
    }

    static String jsonDump(Object obj) {
        return JsonOutput.prettyPrint(JsonOutput.toJson(obj))
    }

    static void printDump(Object obj) {
        // Cannot reference logger directly due to this being a static method.
        /* groovylint-disable-next-line UnnecessaryGetter */
        getLogger().warn(jsonDump(obj))
    }

    static HttpURLConnection connectAndFollowRedirects(String id, String sourceUrl) {
        URL urlObj = new URL(sourceUrl)
        HttpURLConnection connection
        for (int i = 0; i < 10; ++i) {
            // Several deps use this URL for their license, but it just points to license
            // *template*. Generally the actual license can be found in the source code.
            if (sourceUrl.contains('://opensource.org/licenses')) {
                throw new RuntimeException('Found templated license URL for dependency '
                    + id + ': ' + sourceUrl
                    + '. You will need to edit PROPERTY_OVERRIDES for this dep.')
            }
            connection = urlObj.openConnection()
            switch (connection.responseCode) {
                case HttpURLConnection.HTTP_MOVED_PERM:
                case HttpURLConnection.HTTP_MOVED_TEMP:
                    String location = connection.getHeaderField('Location')
                    urlObj = new URL(urlObj, location)
                    continue
                case HttpURLConnection.HTTP_OK:
                    return connection
                default:
                    throw new RuntimeException("Url had statusCode=$connection.responseCode: $sourceUrl")
            }
        }
        throw new RuntimeException("Url in redirect loop: $sourceUrl")
    }

    static void downloadFile(String id, String sourceUrl, File destinationFile) {
        destinationFile.withOutputStream { out ->
            try {
                out << connectAndFollowRedirects(id, sourceUrl).inputStream
            } catch (Exception e) {
                throw new RuntimeException("Failed to fetch license for $id url: $sourceUrl", e)
            }
        }
    }

    @TaskAction
    void main() {
        // Do not run task on subprojects.
        if (project != project.rootProject) {
            return
        }

        skipLicenses = skipLicenses ?: project.hasProperty('skipLicenses')

        Path fetchTemplatePath = Paths.get(sourceUri).resolveSibling('3ppFetch.template')
        Template fetchTemplate = new SimpleTemplateEngine().createTemplate(fetchTemplatePath.toFile())

        Set<Project> subprojects = [] as Set
        subprojects.add(project)
        subprojects.addAll(project.subprojects)
        ChromiumDepGraph graph = new ChromiumDepGraph(
                projects: subprojects, logger: project.logger, skipLicenses: skipLicenses)
        String normalisedRepoPath = normalisePath(repositoryPath)

        // 1. Parse the dependency data
        graph.collectDependencies()

        // 2. Import artifacts into the local repository
        List<String> dependencyDirectories = []
        ExecutorService downloadExecutor = Executors.newCachedThreadPool()
        List<Future> downloadTasks = []
        List<ChromiumDepGraph.DependencyDescription> mergeLicensesDeps = []
        graph.dependencies.values().each { dependency ->
            if (excludeDependency(dependency) || computeJavaGroupForwardingTarget(dependency) != null) {
                return
            }

            ChromiumDepGraph.DependencyDescription dependencyForLogging = dependency.clone()
            // jsonDump() throws StackOverflowError for ResolvedArtifact.
            dependencyForLogging.artifact = null

            logger.debug "Processing ${dependency.name}: \n${jsonDump(dependencyForLogging)}"
            String depDir = computeDepDir(dependency)
            String absoluteDepDir = "${normalisedRepoPath}/${depDir}"

            dependencyDirectories.add(depDir)

            if (new File("${absoluteDepDir}/${dependency.fileName}").exists()) {
                logger.quiet("${dependency.id} exists, skipping.")
                return
            }

            project.copy {
                from dependency.artifact.file
                into absoluteDepDir
            }

            new File("${absoluteDepDir}/README.chromium").write(makeReadme(dependency))
            new File("${absoluteDepDir}/cipd.yaml").write(makeCipdYaml(dependency, cipdBucket, repositoryPath))
            new File("${absoluteDepDir}/OWNERS").write(makeOwners())

            // Enable 3pp flow for //third_party/android_deps only.
            // TODO(crbug.com/1132368): Enable 3pp flow for subprojects as well.
            if (repositoryPath == 'third_party/android_deps') {
                if (dependency.fileUrl) {
                    String absoluteDep3ppDir = "${absoluteDepDir}/3pp"
                    new File(absoluteDep3ppDir).mkdirs()
                    new File("${absoluteDep3ppDir}/3pp.pb").write(make3ppPb(dependency, cipdBucket, repositoryPath))
                    File fetchFile = new File("${absoluteDep3ppDir}/fetch.py")
                    fetchFile.write(make3ppFetch(fetchTemplate, dependency))
                    fetchFile.setExecutable(true, false)
                } else {
                    throw new RuntimeException("Failed to generate 3pp files for ${dependency.id} with empty fileUrl.")
                }
            }

            if (!skipLicenses) {
                validateLicenses(dependency)
                downloadLicenses(dependency, normalisedRepoPath, downloadExecutor, downloadTasks)
                mergeLicensesDeps.add(dependency)
            }
        }
        downloadExecutor.shutdown()
        // Check for exceptions.
        for (Future task : downloadTasks) {
            task.get()
        }

        mergeLicensesDeps.each { dependency ->
            mergeLicenses(dependency, normalisedRepoPath)
        }

        validateDependencies(graph.dependencies.values())

        // 3. Generate the root level build files
        updateBuildTargetDeclaration(graph, normalisedRepoPath)
        if (!ignoreDEPS) {
            updateDepsDeclaration(graph, cipdBucket, repositoryPath,
                                  "${normalisedRepoPath}/../../DEPS")
        }
        dependencyDirectories.sort { path1, path2 -> return path1 <=> path2 }
        updateReadmeReferenceFile(dependencyDirectories,
                                  "${normalisedRepoPath}/additional_readme_paths.json")
    }

    void appendBuildTarget(ChromiumDepGraph.DependencyDescription dependency,
                           Map<String, ChromiumDepGraph.DependencyDescription> allDependencies,
                           StringBuilder sb) {
        if (excludeDependency(dependency) || !dependency.generateTarget) {
            return
        }

        String targetName = translateTargetName(dependency.id) + '_java'
        String javaGroupTarget = computeJavaGroupForwardingTarget(dependency)
        if (javaGroupTarget != null) {
            assert dependency.extension == 'jar' || dependency.extension == 'aar'
            sb.append("""
                java_group("${targetName}") {
                  deps = [ "${javaGroupTarget}" ]
                """.stripIndent())
            if (dependency.testOnly) {
                sb.append('  testonly = true\n')
            }
            sb.append('}\n\n')
            return
                }

        String depsStr = ''
        dependency.children?.each { childDep ->
            ChromiumDepGraph.DependencyDescription dep = allDependencies[childDep]
            if (dep.exclude) {
                return
            }
            // Special case: If a child dependency is an existing lib, rather than skipping
            // it, replace the child dependency with the existing lib.
            String existingLib = EXISTING_LIBS.get(dep.id)
            String depTargetName = translateTargetName(dep.id) + '_java'
            if (existingLib) {
                depsStr += "\"${existingLib}\","
            } else if (excludeDependency(dep)) {
                String thirdPartyDir = (dep.id.startsWith('androidx')) ? 'androidx' : 'android_deps'
                depsStr += "\"//third_party/${thirdPartyDir}:${depTargetName}\","
            } else if (dep.id == 'com_google_android_material_material') {
                // Material design is pulled in via doubledown, should
                // use the variable instead of the real target.
                depsStr += '"$material_design_target",'
            } else {
                depsStr += "\":${depTargetName}\","
            }
        }

        String libPath = "${DOWNLOAD_DIRECTORY_NAME}/${dependency.directoryName}"
        sb.append(GEN_REMINDER)
        if (dependency.extension == 'jar') {
            sb.append("""\
                java_prebuilt("${targetName}") {
                  jar_path = "${libPath}/${dependency.fileName}"
                  output_name = "${dependency.id}"
                """.stripIndent())
            if (dependency.supportsAndroid) {
                sb.append('  supports_android = true\n')
            } else {
                // Save some time by not validating classpaths of desktop .jars. Also required to break a dependency
                // cycle for errorprone.
                sb.append('  enable_bytecode_checks = false\n')
            }
        } else if (dependency.extension == 'aar') {
            sb.append("""\
                android_aar_prebuilt("${targetName}") {
                  aar_path = "${libPath}/${dependency.fileName}"
                  info_path = "${libPath}/${dependency.id}.info"
            """.stripIndent())
        } else {
            throw new IllegalStateException('Dependency type should be JAR or AAR')
                }

        sb.append(generateBuildTargetVisibilityDeclaration(dependency))

        if (dependency.testOnly) {
            sb.append('  testonly = true\n')
        }
        if (!depsStr.empty) {
            sb.append("  deps = [${depsStr}]\n")
        }
        addSpecialTreatment(sb, dependency.id, dependency.extension)

        sb.append('}\n\n')
                }

    String generateBuildTargetVisibilityDeclaration(ChromiumDepGraph.DependencyDescription dependency) {
        StringBuilder sb = new StringBuilder()
        switch (dependency.id) {
            case 'com_google_android_material_material':
                sb.append('  # Material Design is pulled in via Doubledown, thus this target should not\n')
                sb.append('  # be directly depended on. Please use :material_design_java instead.\n')
                sb.append(generateInternalTargetVisibilityLine())
                return sb.toString()
            case 'com_google_protobuf_protobuf_javalite':
                sb.append('  # Protobuf runtime is pulled in via Doubledown, thus this target should not\n')
                sb.append('  # be directly depended on. Please use :protobuf_lite_runtime_java instead.\n')
                sb.append(generateInternalTargetVisibilityLine())
                return sb.toString()
        }

        if (!dependency.visible) {
            sb.append('  # To remove visibility constraint, add this dependency to\n')
            sb.append("  # //${repositoryPath}/build.gradle.\n")
            sb.append(generateInternalTargetVisibilityLine())
        }
        return sb.toString()
    }

    boolean excludeDependency(ChromiumDepGraph.DependencyDescription dependency) {
        if (dependency.exclude || EXISTING_LIBS.get(dependency.id)) {
            return true
        }
        boolean isAndroidxRepository = (repositoryPath == 'third_party/androidx')
        boolean isAndroidxDependency = (dependency.id.startsWith('androidx'))
        if (isAndroidxRepository != isAndroidxDependency) {
            return true
        }
        if (repositoryPath == AUTOROLLED_REPO_PATH) {
            String targetName = translateTargetName(dependency.id) + '_java'
            return !isTargetAutorolled(targetName)
        }
        // TODO(crbug.com/1184780): Remove this once org_robolectric_shadows_multidex is updated to a newer version
        // which does not need jetify.
        if (dependency.directoryName == 'org_robolectric_shadows_multidex') {
            if (dependency.version != '4.3.1') {
                throw new RuntimeException('Got a new version for org_robolectric_shadows_multidex. If this new ' +
                                           "version doesn't need jetify, please move this dependency back to the " +
                                           'auto-generated section in //DEPS and //third_party/android_deps/BUILD.gn.')
            }
            return true
        }
        return false
    }

    /** If |dependency| should be a java_group(), returns target to forward to. Returns null otherwise. */
    String computeJavaGroupForwardingTarget(ChromiumDepGraph.DependencyDescription dependency) {
        String targetName = translateTargetName(dependency.id) + '_java'
        return repositoryPath != AUTOROLLED_REPO_PATH && isTargetAutorolled(targetName) ?
               "//${AUTOROLLED_REPO_PATH}:${targetName}" : null
    }

    private static String makeGnArray(String[] values) {
        StringBuilder sb = new StringBuilder()
        sb.append('[')
        for (String value : values) {
            sb.append('"')
            sb.append(value)
            sb.append('",')
        }
        sb.replace(sb.length() - 1, sb.length(), ']')
        return sb.toString()
    }

    private static String computeDepDir(ChromiumDepGraph.DependencyDescription dependency) {
        return "${DOWNLOAD_DIRECTORY_NAME}/${dependency.directoryName}"
    }

    /* groovylint-disable-next-line MethodSize */
    private static void addSpecialTreatment(StringBuilder sb, String dependencyId, String dependencyExtension) {
        addPreconditionsOverrideTreatment(sb, dependencyId)

        if (dependencyId.startsWith('org_robolectric')) {
            // Skip platform checks since it depends on
            // accessibility_test_framework_java which requires_android.
            sb.append('  bypass_platform_checks = true\n')
        }
        if (dependencyExtension == 'aar' &&
                (dependencyId.startsWith('androidx') || dependencyId.startsWith('com_android_support'))) {
            // The androidx and com_android_support libraries have duplicate resources such as
            // 'primary_text_default_material_dark'.
            sb.append('  resource_overlay = true\n')
                }

        switch (dependencyId) {
            case 'androidx_annotation_annotation':
                sb.append('  # https://crbug.com/989505\n')
                sb.append('  jar_excluded_patterns = ["META-INF/proguard/*"]\n')
                break
            case 'androidx_annotation_annotation_experimental':
                sb.append('''\
                |  # https://crbug.com/1213876
                |  deps =
                |      [ "//third_party/android_deps:org_jetbrains_kotlin_kotlin_stdlib_java" ]
                |'''.stripMargin())
                break
            case 'androidx_core_core':
                sb.with {
                    append('\n')
                    append('  # Target has AIDL, but we do not support it yet: http://crbug.com/644439\n')
                    append('  ignore_aidl = true\n')
                    append('\n')
                    append('  # Manifest and proguard config have just one entry: Adding (and -keep\'ing\n')
                    append('  # android:appComponentFactory="androidx.core.app.CoreComponentFactory"\n')
                    append('  # Chrome does not use this feature and it causes a scary stack trace to be\n')
                    append('  # shown when incremental_install=true.\n')
                    append('  ignore_manifest = true\n')
                    append('  ignore_proguard_configs = true\n')
                }
                break
            case 'androidx_fragment_fragment':
                sb.append('''\
                |  deps += [
                |    "//third_party/android_deps/utils:java",
                |  ]
                |
                |  proguard_configs = ["androidx_fragment.flags"]
                |
                |  bytecode_rewriter_target = "//build/android/bytecode:fragment_activity_replacer"
                |'''.stripMargin())
                break
            case 'androidx_media_media':
            case 'androidx_versionedparcelable_versionedparcelable':
            case 'com_android_support_support_media_compat':
                sb.append('\n')
                sb.append('  # Target has AIDL, but we do not support it yet: http://crbug.com/644439\n')
                sb.append('  ignore_aidl = true\n')
                break
            case 'androidx_test_uiautomator_uiautomator':
                sb.append('  deps = [":androidx_test_runner_java"]\n')
                break
            case 'androidx_mediarouter_mediarouter':
                sb.append('  # https://crbug.com/1000382\n')
                sb.append('  proguard_configs = ["androidx_mediarouter.flags"]\n')
                break
            case 'androidx_room_room_runtime':
                sb.append('  enable_bytecode_checks = false\n')
                break
            case 'androidx_transition_transition':
                // Not specified in the POM, compileOnly dependency not supposed to be used unless
                // the library is present: b/70887421
                sb.append('  deps += [":androidx_fragment_fragment_java"]\n')
                break
            case 'androidx_lifecycle_lifecycle_process':
                sb.append('\n')
                sb.append('  # Only useful for very old SDKs.\n')
                sb.append('  ignore_proguard_configs = true\n')
                break
            case 'android_arch_lifecycle_runtime':
            case 'android_arch_lifecycle_viewmodel':
            case 'androidx_lifecycle_lifecycle_runtime':
            case 'androidx_lifecycle_lifecycle_viewmodel':
                sb.append('\n')
                sb.append('  # https://crbug.com/887942#c1\n')
                sb.append('  ignore_proguard_configs = true\n')
                break
            case 'com_android_support_coordinatorlayout':
            case 'androidx_coordinatorlayout_coordinatorlayout':
                sb.append('\n')
                sb.append('  # Reduce binary size. https:crbug.com/954584\n')
                sb.append('  ignore_proguard_configs = true\n')
                break
            case 'com_google_android_material_material':
                sb.append('\n')
                sb.append('  # Reduce binary size. https:crbug.com/954584\n')
                sb.append('  ignore_proguard_configs = true\n')
                break
            case 'com_android_support_support_annotations':
                sb.append('  # https://crbug.com/989505\n')
                sb.append('  jar_excluded_patterns = ["META-INF/proguard/*"]\n')
                break
            case 'com_android_support_support_compat':
                sb.append('\n')
                sb.append('  # Target has AIDL, but we do not support it yet: http://crbug.com/644439\n')
                sb.append('  ignore_aidl = true\n')
                sb.append('  ignore_manifest = true\n')
                // Necessary to not have duplicate classes after jetification.
                // They can be removed when we no longer jetify targets
                // that depend on com_android_support_support_compat.
                sb.append('''\
                |  jar_excluded_patterns = [
                |    "android/support/v4/graphics/drawable/IconCompatParcelizer.class",
                |    "android/support/v4/os/ResultReceiver*",
                |    "androidx/core/graphics/drawable/IconCompatParcelizer.class",
                |    "androidx/core/internal/package-info.class",
                |    "android/support/v4/app/INotificationSideChannel*",
                |    "android/support/v4/os/IResultReceiver*",
                |  ]
                |
                |'''.stripMargin())
                break
            case 'com_android_support_transition':
                // Not specified in the POM, compileOnly dependency not supposed to be used unless
                // the library is present: b/70887421
                sb.append('  deps += [":com_android_support_support_fragment_java"]\n')
                break
            case 'com_android_support_versionedparcelable':
                sb.append('\n')
                sb.append('  # Target has AIDL, but we do not support it yet: http://crbug.com/644439\n')
                sb.append('  ignore_aidl = true\n')
                // Necessary to not have identical classes after jetification.
                // They can be removed when we no longer jetify targets
                // that depend on com_android_support_versionedparcelable.
                sb.append('''\
                |  jar_excluded_patterns = [
                |    "android/support/v4/graphics/drawable/IconCompat.class",
                |    "androidx/*",
                |  ]
                |
                |'''.stripMargin())
                break
            case 'com_google_ar_core':
                // Target .aar file contains .so libraries that need to be extracted,
                // and android_aar_prebuilt template will fail if it's not set explictly.
                sb.append('  extract_native_libraries = true\n')
                break
            case 'com_google_guava_guava':
                sb.append('\n')
                sb.append('  # Need to exclude class and replace it with class library as\n')
                sb.append('  # com_google_guava_listenablefuture has support_androids=true.\n')
                sb.append('  deps += [":com_google_guava_listenablefuture_java"]\n')
                sb.append('  jar_excluded_patterns = ["*/ListenableFuture.class"]\n')
                break
            case 'com_google_guava_guava_android':
                sb.with {
                    append('\n')
                    append('  # Add a dep to com_google_guava_listenablefuture_java\n')
                    append('  # because androidx_concurrent_futures also depends on it and to avoid\n')
                    append('  # defining ListenableFuture.class twice.\n')
                    append('  deps += [":com_google_guava_listenablefuture_java"]\n')
                    append('  jar_excluded_patterns += ["*/ListenableFuture.class"]\n')
                }
                break
            case 'com_google_code_findbugs_jsr305':
            case 'com_google_errorprone_error_prone_annotations':
            case 'com_google_guava_failureaccess':
            case 'com_google_j2objc_j2objc_annotations':
            case 'com_google_guava_listenablefuture':
            case 'com_googlecode_java_diff_utils_diffutils':
            case 'org_checkerframework_checker_qual':
            case 'org_codehaus_mojo_animal_sniffer_annotations':
                sb.append('\n')
                sb.append('  # Needed to break dependency cycle for errorprone_plugin_java.\n')
                sb.append('  enable_bytecode_checks = false\n')
                break
            case 'androidx_test_rules':
                // Target needs Android SDK deps which exist in third_party/android_sdk.
                sb.append('''\
                |  deps += [
                |    "//third_party/android_sdk:android_test_base_java",
                |    "//third_party/android_sdk:android_test_mock_java",
                |    "//third_party/android_sdk:android_test_runner_java",
                |  ]
                |
                |'''.stripMargin())
                break
            case 'androidx_test_espresso_espresso_contrib':
            case 'androidx_test_espresso_espresso_web':
            case 'androidx_window_window':
                sb.append('  enable_bytecode_checks = false\n')
                break
            case 'net_sf_kxml_kxml2':
                sb.append('  # Target needs to exclude *xmlpull* files as already included in Android SDK.\n')
                sb.append('  jar_excluded_patterns = [ "*xmlpull*" ]\n')
                break
            case 'androidx_preference_preference':
                sb.append('''\
                |  bytecode_rewriter_target = "//build/android/bytecode:fragment_activity_replacer"
                |'''.stripMargin())
                // Replace broad library -keep rules with a more limited set in
                // chrome/android/java/proguard.flags instead.
                sb.append('  ignore_proguard_configs = true\n')
                break
            case 'com_google_android_gms_play_services_base':
                sb.append('  bytecode_rewriter_target = "//build/android/bytecode:fragment_activity_replacer"\n')
                break
            case 'com_google_android_gms_play_services_basement':
                sb.append('  # https://crbug.com/989505\n')
                sb.append('  jar_excluded_patterns += ["META-INF/proguard/*"]\n')
                // Deprecated deps jar but still needed by play services basement.
                sb.append('  input_jars_paths=["\\$android_sdk/optional/org.apache.http.legacy.jar"]\n')
                sb.append('  bytecode_rewriter_target = "//build/android/bytecode:fragment_activity_replacer"\n')
                break
            case 'com_google_android_gms_play_services_maps':
                sb.append('  # Ignore the dependency to org.apache.http.legacy. See crbug.com/1084879.\n')
                sb.append('  ignore_manifest = true\n')
                break
            case 'com_google_protobuf_protobuf_javalite':
                sb.with {
                    append('  # Prebuilt protos in the runtime library.\n')
                    append('  # If you want to use these protos, you should create a proto_java_library\n')
                    append('  # target for them. See crbug.com/1103399 for discussion.\n')
                    append('  jar_excluded_patterns = [\n')
                    append('    "com/google/protobuf/Any*",\n')
                    append('    "com/google/protobuf/Api*",\n')
                    append('    "com/google/protobuf/Duration*",\n')
                    append('    "com/google/protobuf/Empty*",\n')
                    append('    "com/google/protobuf/FieldMask*",\n')
                    append('    "com/google/protobuf/SourceContext*",\n')
                    append('    "com/google/protobuf/Struct\\\\\\$1.class",\n')
                    append('    "com/google/protobuf/Struct\\\\\\$Builder.class",\n')
                    append('    "com/google/protobuf/Struct.class",\n')
                    append('    "com/google/protobuf/StructOrBuilder.class",\n')
                    append('    "com/google/protobuf/StructProto.class",\n')
                    append('    "com/google/protobuf/Timestamp*",\n')
                    append('    "com/google/protobuf/Type*",\n')
                    append('    "com/google/protobuf/Wrappers*",\n')
                    append('  ]')
                }
                break
            case 'androidx_startup_startup_runtime':
                sb.append('  # Keeps emoji2 code. See http://crbug.com/1205141\n')
                sb.append('  ignore_proguard_configs = true\n')
                break
            case 'androidx_webkit_webkit':
                sb.append('  visibility = [\n')
                sb.append('    "//android_webview/tools/system_webview_shell:*",\n')
                sb.append('    "//third_party/android_deps:*"\n')
                sb.append('  ]')
                break
            case 'com_android_tools_desugar_jdk_libs_configuration':
                sb.append('  enable_bytecode_checks = false\n')
                break
            case 'com_google_firebase_firebase_common':
                sb.append('\n')
                sb.append('  # Ignore missing kotlin.KotlinVersion definition in\n')
                sb.append('  # com.google.firebase.platforminfo.KotlinDetector.\n')
                sb.append('  enable_bytecode_checks = false\n')
                break
            case 'com_google_firebase_firebase_components':
                sb.append('\n')
                sb.append('  # Can\'t find com.google.firebase.components.Component\\$ComponentType.\n')
                sb.append('  enable_bytecode_checks = false\n')
                break
            case 'com_google_firebase_firebase_installations':
            case 'com_google_firebase_firebase_installations_interop':
                sb.append('\n')
                sb.append('  # Can\'t find com.google.auto.value.AutoValue\\$Builder.\n')
                sb.append('  enable_bytecode_checks = false\n')
                break
            case 'com_google_firebase_firebase_messaging':
                sb.append('\n')
                sb.append('  # We removed the datatransport dependency to reduce binary size.\n')
                sb.append('  # The library works without it as it\'s only used for logging.\n')
                sb.append('  enable_bytecode_checks = false\n')
                break
            case 'com_android_tools_sdk_common':
            case 'com_android_tools_common':
            case 'com_android_tools_layoutlib_layoutlib_api':
                sb.append('\n')
                sb.append('  # This target does not come with most of its dependencies and is\n')
                sb.append('  # only meant to be used by the resources shrinker. If you wish to use\n')
                sb.append('  # this for other purposes, change buildCompileNoDeps in build.gradle.\n')
                sb.append('  visibility = [ "//build/android/unused_resources:*" ]\n')
                break
            case 'org_jetbrains_kotlinx_kotlinx_coroutines_android':
                sb.append('requires_android = true')
                break
        }
    }

    private static void addPreconditionsOverrideTreatment(StringBuilder sb, String dependencyId) {
        String targetName = translateTargetName(dependencyId)
        switch (targetName) {
          case 'androidx_core_core':
          case 'com_google_guava_guava_android':
          case 'google_play_services_basement':
                if (targetName == 'com_google_guava_guava_android') {
                    // com_google_guava_guava_android is java_prebuilt().
                    sb.append('bypass_platform_checks = true')
                }
                String libraryDep = '//third_party/android_deps/local_modifications/preconditions:' +
                        computePreconditionsStubLibraryForDep(dependencyId)
                sb.append("""
                |
                | jar_excluded_patterns = []
                | if (!enable_java_asserts) {
                |   # Omit the file since we use our own copy.
                |   jar_excluded_patterns += [
                |     "${computePreconditionsClassForDep(dependencyId)}",
                |   ]
                |   deps += [
                |     "${libraryDep}",
                |   ]
                | }
                |""".stripMargin())
        }
    }

    private static String computePreconditionsStubLibraryForDep(String dependencyId) {
        String targetName = translateTargetName(dependencyId)
        switch (targetName) {
            case 'androidx_core_core':
                return 'androidx_stub_preconditions_java'
            case 'com_google_guava_guava_android':
                return 'guava_stub_preconditions_java'
            case 'google_play_services_basement':
                return 'gms_stub_preconditions_java'
        }
        return null
    }

    private static String computePreconditionsClassForDep(String dependencyId) {
        String targetName = translateTargetName(dependencyId)
        switch (targetName) {
            case 'androidx_core_core':
                return 'androidx/core/util/Preconditions.class'
            case 'com_google_guava_guava_android':
                return 'com/google/common/base/Preconditions.class'
            case 'google_play_services_basement':
                return 'com/google/android/gms/common/internal/Preconditions.class'
        }
        return null
    }

    private static void updateReadmeReferenceFile(List<String> directories, String readmePath) {
        File refFile = new File(readmePath)
        refFile.write(JsonOutput.prettyPrint(JsonOutput.toJson(directories)) + '\n')
    }

    private void updateBuildTargetDeclaration(ChromiumDepGraph depGraph, String normalisedRepoPath) {
        File buildFile = new File("${normalisedRepoPath}/BUILD.gn")
        StringBuilder sb = new StringBuilder()

        // Comparator to sort the dependency in alphabetical order, with the visible ones coming
        // before all the internal ones.
        Closure dependencyComparator = { dependency1, dependency2 ->
            int visibilityResult = Boolean.compare(dependency1.visible, dependency2.visible)
            if (visibilityResult != 0) {
                return -visibilityResult
            }
            return dependency1.id <=> dependency2.id
        }

        List<ChromiumDepGraph.DependencyDescription> buildCompileDependencies
        buildCompileDependencies = depGraph.dependencies.values().findAll {
            dependency -> dependency.usedInBuild
        }

        buildCompileDependencies.sort(dependencyComparator).each { dependency ->
            appendBuildTarget(dependency, depGraph.dependencies, sb)
        }

        sb.append('if (!limit_android_deps) {\n')
        List<ChromiumDepGraph.DependencyDescription> otherDependencies
        otherDependencies = depGraph.dependencies.values().findAll {
            dependency -> !dependency.usedInBuild
        }
        otherDependencies.sort(dependencyComparator).each { dependency ->
            appendBuildTarget(dependency, depGraph.dependencies, sb)
        }
        sb.append('}\n')

        String out = "${BUILD_GN_TOKEN_START}\n$sb\n${BUILD_GN_TOKEN_END}"
        if (buildFile.exists()) {
            Matcher matcher = BUILD_GN_GEN_PATTERN.matcher(buildFile.text)
            if (!matcher.find()) {
                throw new IllegalStateException('BUILD.gn insertion point not found.')
            }
            out = matcher.replaceFirst(out)
        } else {
            out = 'import("//build/config/android/rules.gni")\n' + out
        }
        buildFile.write(out)
    }

    private void validateDependencies(
            Collection<ChromiumDepGraph.DependencyDescription> dependencies) {
        dependencies.each { dependency ->
            if (dependency.id.contains('androidx') && !dependency.fileName.contains('SNAPSHOT')) {
                boolean hasAllowedDep = ALLOWED_ANDROIDX_NON_SNAPSHOT_DEPS_PREFIXES.any {
                    allowedPrefix -> dependency.id.startsWith(allowedPrefix)
                }
                if (!hasAllowedDep) {
                    String errorMsg = ("${dependency.fileName} uses non-SNAPSHOT version."
                          + "If this is expected, add ${dependency.id} to "
                          + '|ALLOWED_ANDROIDX_NON_SNAPSHOT_DEPS_PREFIXES| list.')
                    throw new IllegalStateException(errorMsg)
                }
            }
        }
    }

    private String generateInternalTargetVisibilityLine() {
        return "visibility = ${makeGnArray(internalTargetVisibility)}\n"
    }

    private void updateDepsDeclaration(ChromiumDepGraph depGraph, String cipdBucket,
                                       String repoPath, String depsFilePath) {
        File depsFile = new File(depsFilePath)
        StringBuilder sb = new StringBuilder()
        // Note: The string we're inserting is nested 1 level, hence the 2 leading spaces. Same
        // applies to the multiline package declaration string below.
        sb.append('  # Generated by //third_party/android_deps/fetch_all.py')

        // Comparator to sort the dependencies in alphabetical order.
        Closure dependencyComparator = { dependency1, dependency2 ->
            dependency1.id <=> dependency2.id
        }

        depGraph.dependencies.values().sort(dependencyComparator).each { dependency ->
            if (excludeDependency(dependency) || computeJavaGroupForwardingTarget(dependency)) {
                return
            }
            String depPath = "${DOWNLOAD_DIRECTORY_NAME}/${dependency.directoryName}"
            String cipdPath = "${cipdBucket}/${repoPath}/${depPath}"
            sb.append("""\
            |
            |  'src/${repoPath}/${depPath}': {
            |      'packages': [
            |          {
            |              'package': '${cipdPath}',
            |              'version': 'version:${THREEPP_EPOCH}@${dependency.version}.${dependency.cipdSuffix}',
            |          },
            |      ],
            |      'condition': 'checkout_android',
            |      'dep_type': 'cipd',
            |  },
            |""".stripMargin())
        }

        Matcher matcher = DEPS_GEN_PATTERN.matcher(depsFile.text)
        if (!matcher.find()) {
            throw new IllegalStateException('DEPS insertion point not found.')
        }
        depsFile.write(matcher.replaceFirst("${DEPS_TOKEN_START}\n${sb}\n  ${DEPS_TOKEN_END}"))
                                       }

    private boolean isTargetAutorolled(String targetName) {
        for (String autorolledLibPrefix in AUTOROLLED_LIB_PREFIXES) {
            if (targetName.startsWith(autorolledLibPrefix)) {
                return true
            }
        }
        return false
    }

    private String normalisePath(String pathRelativeToChromiumRoot) {
        return project.file("${chromiumSourceRoot}/${pathRelativeToChromiumRoot}").absolutePath
    }

}
