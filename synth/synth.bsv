$IMPORTS

interface SYNTH;
    (* prefix="_", result = "out" *)
    method $RETURN_TYPE ${FUNC_NAME}_ ($FUNC_ARGS);
endinterface

(* synthesize *)
module mkSynth(SYNTH);
    method ${FUNC_NAME}_ = ${FUNC_NAME};
endmodule
