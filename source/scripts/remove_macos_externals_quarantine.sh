echo "\
In general, it's better to run codesigned and notarized software 
from trusted sources.

In the Apple ecosystem, in order to become a trusted source for externally
downloadable software, you have to pay for an Apple developer account which
costs 100 USD per year, and then use credentials to sign software and get
your software notarized by Apple.

Any software that is downloaded from external sources is automatically
put under quarantine by macOS.

If you are building externals yourself this quarantine is not applied.or
However, if you are using github actions where you can transparently read
the code that is built remotely, you can optionally choose to remove this
quarantine by running the following commandline instructions:

codesign --sign - --timestamp --force externals/*.mxo/**/MacOS/*
/usr/bin/xattr -r -d com.apple.quarantine externals/*.mxo

codesign --sign - --timestamp --force externals/pd/**/*.pd_darwin
/usr/bin/xattr -r -d com.apple.quarantine externals/pd/**/*.pd_darwin

What does codesign --sign - do?

This is called an ad-hoc codesign. 
see: https://developer.apple.com/documentation/security/seccodesignatureflags/adhoc
see: https://github.com/Cycling74/min-devkit

What does xattr -r -d com.apple.quarantine do?

That command causes the OS to skip the malware check for the file specified;
it normally gets removed if it doesn't find any in it. Running it should only
be done if the computer falsely detects some and it's from a trusted source.

Ultimately it's up to you to decide if you are going to trust this software
that you have downloaded.
"
codesign --sign - --timestamp --force externals/*.mxo/**/MacOS/*
/usr/bin/xattr -r -d com.apple.quarantine externals/*.mxo

