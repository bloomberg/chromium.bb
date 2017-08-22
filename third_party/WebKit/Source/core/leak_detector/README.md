# Leak Detector

This directory contains the implementation of Blink Leak Detector and Blink Leak Detector Client.
Leak detection here means identifying leaks on renderer after a page navigation.
Blink Leak Detector class implements the preparation method and the GC method to stabilize the detection results.
Blink Leak Detector class can be inherited to define the operations after leak detection.
