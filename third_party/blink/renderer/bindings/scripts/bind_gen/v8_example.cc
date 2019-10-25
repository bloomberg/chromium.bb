void AttributeGetCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ScriptState* current_script_state = ScriptState::ForCurrentRealm(info);
  ScriptState* script_state = current_script_state;
  V8PerContextData* per_context_data = script_state->PerContextData();
  v8::Isolate* isolate = info.GetIsolate();
  const ExceptionState::ContextType exception_state_context_type =
      ExceptionState::kGetterContext;
  const char* const class_like_name = "TestNamespace";
  const char* const property_name = "attr1";
  ExceptionState exception_state(isolate, exception_state_context_type,
                                 class_like_name, property_name);
  ExceptionToRejectPromiseScope reject_promise_scope(info, exception_state);
  (void)(per_context_data, exception_state);
}
