add_library(hobbeslang
  closcvt.C
  constraints.C
  expr.C
  macroexpand.C
  module.C
  type.C
  typeinf.C
  typemap.C
  typepreds.C
  tyunqualify.C
  pat/dfa.C
  pat/pattern.C
  pat/print.C
  pat/regex.C
  preds/appendsto/record.C
  preds/appendsto.C
  preds/class.C
  preds/consrecord.C
  preds/consvariant.C
  preds/data.C
  preds/deconstruct.C
  preds/equal.C
  preds/hasctor/variant.C
  preds/hasctor.C
  preds/hasfield/lookup.C
  preds/hasfield/record.C
  preds/hasfield/slookup.C
  preds/hasfield/tenvlookup.C
  preds/hasfield.C
  preds/loadref.C
  preds/not.C
  preds/packsto.C
  preds/recty.C
  preds/sizeof.C
  preds/str.C
  preds/subtype/obj.C
  preds/subtype.C
  preds/typeof.C
  )

target_include_directories(hobbeslang
  PUBLIC
  ${HOBBES_MAIN_INCLUDE_DIR}/hobbes/lang/
  )

install(TARGETS hobbeslang DESTINATION "lib")

