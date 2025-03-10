%module(package="aether") ventilation
%include symbol_export.h
%include ventilation.h

%{
#include "ventilation.h"
%}

// Alternatively, you can use this typemap if needed for more complex handling
%typemap(out) double evaluate_vent {
    $result = PyFloat_FromDouble($1);
}