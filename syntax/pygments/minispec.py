from pygments.lexer import RegexLexer, bygroups, words
from pygments.token import *

__all__ = ['MinispecLexer']

class MinispecLexer(RegexLexer):
    """
    For Minispec HDL
    Based on Pygment's verilog lexer, at https://bitbucket.org/birkenfeld/pygments-main/src/default/pygments/lexers/hdl.py
    Author: Daniel Sanchez
    """
    name = 'minispec'
    aliases = ['minispec', 'ms']
    filenames = ['*.ms']
    mimetypes = ['text/x-minispec']

    #: optional Comment or Whitespace
    _ws = r'(?:\s|//.*?\n|/[*].*?[*]/)+'

    tokens = {
        'root': [
            (r'\n', Text),
            (r'\s+', Text),
            (r'\\\n', Text),  # line continuation
            (r'/(\\\n)?/(\n|(.|\n)*?[^\\]\n)', Comment.Single),
            (r'/(\\\n)?[*](.|\n)*?[*](\\\n)?/', Comment.Multiline),
            (r'[{}#]', Punctuation),
            (r'L?"', String, 'string'),
            (r"L?'(\\.|\\[0-7]{1,3}|\\x[a-fA-F0-9]{1,2}|[^\\\'\n])'", String.Char),
            (r'([0-9]+)|(\'h)[0-9a-fA-F_]+', Number.Hex),
            (r'([0-9]+)|(\'b)[01_]+', Number.Bin),
            (r'([0-9]+)|(\'d)[0-9_]+', Number.Integer),
            (r'([0-9]+)|(\'o)[0-7_]+', Number.Oct),
            (r'\d+[Ll]?', Number.Integer),
            (r'\*/', Error),
            (r'[~!%^&*+=|?:<>/-]', Operator),
            (r'[()\[\],.;\']', Punctuation),

            (r'^(\s*)(import)(\s+)', bygroups(Text, Keyword.Namespace, Text),
             'import'),
            (r'^(\s*)(bsvimport)(\s+)', bygroups(Text, Keyword.Namespace, Text),
             'import'),

            (words((
                'begin', 'bsvimport', 'case', 'default', 'else', 'end', 'endcase',
                'endfunction', 'endmethod', 'endmodule', 'endrule', 'enum', 'for',
                'function', 'if', 'import', 'input', 'let', 'method', 'module',
                'return', 'rule', 'struct', 'type', 'typedef'), suffix=r'\b'),
             Keyword),

            (words(('False', 'Invalid', 'True', 'Valid'), suffix=r'\b'),
             Keyword.Constant),

            (r'\$\w*', Name.Builtin),
            
            (r'[A-Z]\w*', Keyword.Type),

            (r'[a-z]\w*:(?!:)', Name.Label),
            (r'\$?[a-zA-Z_]\w*', Name),
        ],
        'string': [
            (r'"', String, '#pop'),
            (r'\\([\\abfnrtv"\']|x[a-fA-F0-9]{2,4}|[0-7]{1,3})', String.Escape),
            (r'[^\\"\n]+', String),  # all other characters
            (r'\\\n', String),  # line continuation
            (r'\\', String),  # stray backslash
        ],
        'import': [
            (r'[\w:]+\*?', Name.Namespace, '#pop')
        ]
    }

    def get_tokens_unprocessed(self, text):
        for index, token, value in \
                RegexLexer.get_tokens_unprocessed(self, text):
            # Convention: mark all upper case names as constants
            if token is Name:
                if value.isupper():
                    token = Name.Constant
            yield index, token, value
