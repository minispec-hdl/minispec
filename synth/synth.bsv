$IMPORTS

interface SYNTH;
    (* prefix="_", result = "out" *)
    $METHOD
endinterface

(* synthesize *)
module mkSynth(SYNTH);
    $METHOD
        return $FUNCCALL;
    endmethod
endmodule
