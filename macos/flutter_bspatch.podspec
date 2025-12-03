#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint flutter_bspatch.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'flutter_bspatch'
  s.version          = '0.0.1'
  s.summary          = 'Flutter bspatch plugin for applying binary patches.'
  s.description      = <<-DESC
A Flutter FFI plugin that provides bspatch functionality for applying binary differential patches.
                       DESC
  s.homepage         = 'https://github.com/li-zenghui/flutter_bspatch'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Your Company' => 'email@example.com' }

  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'

  s.dependency 'FlutterMacOS'

  s.platform = :osx, '10.11'
  
  # 头文件搜索路径
  s.xcconfig = {
    'HEADER_SEARCH_PATHS' => [
      '"$(PODS_TARGET_SRCROOT)/../src"',
      '"$(PODS_TARGET_SRCROOT)/../src/third_party/bzip2"',
      '"$(PODS_TARGET_SRCROOT)/../src/third_party/bsdiff"'
    ].join(' '),
    'GCC_PREPROCESSOR_DEFINITIONS' => 'BZ_NO_STDIO=1'
  }

  s.pod_target_xcconfig = { 
    'DEFINES_MODULE' => 'YES',
    'GCC_WARN_INHIBIT_ALL_WARNINGS' => 'YES'
  }
  s.swift_version = '5.0'
end
