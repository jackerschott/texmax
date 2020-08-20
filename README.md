# texmax
> This is a work in progress. Use at your own risk.

## Introduction
Texmax is a minimal frontend for the free computer algebra system maxima.
It uses LaTeX for output rendering while maxima commands are supplied via a
named pipe.
This program is designed to follow the unix philosophy and thus allow for easy
integration into other software, while the main purpose is, of course, the
integration into extensible text editors.

## Usage
If you run texmax it creates the following directories/files

    .texmax/
        cmd
        doc.tex
        res.tex
        max.log

Among these files `cmd` is a named pipe which can be used to send a command to
texmax, for example

    $ echo "com\nintegrate(1/x,x);" > .texmax/cmd

texmax then evaluates the action `com` with the argument `integrate(1/x,x);`,
which means "compute the expression `integrate(1/x,x);` via maxima".
The output will then be appended to `res.tex` which content gets included in
`doc.tex`, such that `doc.tex` can be compiled via LaTeX.

Once texmax is finished a return code will be written to `cmd` which shows 
potential errors while also indicating that `doc.tex` is ready for compilation.
A return code of `0` declares that texmax finished successfully, while a
positive value declares that an error occurred.
This happens for example if the semicolon after `integrate(1/x,x)` would be
missing.
One can retrieve the return code, while simultaneously waiting for texmax, for
example, with
    
    $ cat .texmax/cmd


Aside from the action `com` there is also `bat` to evaluate a file (the filename
is given as an argument) with maxima commands line by line, `rst` to restart
maxima and `end` to terminate texmax (the argument is ignored for the last two
actions).

The file `max.log` simply serves as a log file for the maxima text output, which
can be consulted if something goes wrong with the LaTeX version.

## Examples
### Integration in vim
To integrate texmax in vim add something like the following to your vimrc
```vim
function! TexmaxExecLine()
  let s:line = getline(".")
  silent exec "!echo \"com\\n" . escape(getline("."), "%\"") . "\" > .texmax/cmd"
  silent exec "!cat .texmax/cmd >/dev/null"
  silent exec "!latexmk -pdf -interaction=nonstopmode -output-directory=.tex .texmax/doc.tex"
endfunction

function! TexmaxExecFile()
  let s:line = getline(".")
  silent exec "!echo \"bat\\n".@%."\" > .texmax/cmd"
  silent exec "!cat .texmax/cmd >/dev/null"
  silent exec "!latexmk -pdf -interaction=nonstopmode -output-directory=.tex .texmax/doc.tex"
endfunction

autocmd BufRead *.mac command! TexmaxStart silent exec "!texmax &"
autocmd BufRead *.mac command! TexmaxStop silent exec "!echo \"end\" > .texmax/cmd"
autocmd BufRead *.mac command! TexmaxRestartMaxima silent exec "!echo \"rst\" > .texmax/cmd"
autocmd BufRead *.mac nnoremap <space> :call TexmaxExecLine()<cr>
autocmd BufRead *.mac nnoremap <enter> :call TexmaxExecFile()<cr>
autocmd BufRead *.mac nnoremap <leader>v :!mimeo .tex/doc.pdf<cr><cr>
autocmd BufRead *.mac nnoremap <leader>vd :split .texmax/doc.tex<cr>
autocmd BufRead *.mac nnoremap <leader>vr :split .texmax/res.tex<cr>
autocmd BufRead *.mac nnoremap <leader>vl :split .texmax/max.log<cr>
autocmd BufUnload *.mac silent exec "!test -p .texmax/cmd && echo \"end\" > .texmax/cmd"
```

## Installation
### Platforms
Texmax is currently only being tested on linux.

### Requirements
To use texmax you should install `dmenu` (for question prompts in maxima) and
probably a LaTeX compiler, otherwise it will be pretty useless.

### Building
To build texmax use

    $ make

For installation and deinstallation use

    # make install

and

    # make uninstall
