static char cmd[] = 
"# $Id: s.predefined.xotcl 1.22 01/05/15 18:05:33+02:00 neumann@mohegan.wu-wien.ac.at $\n"
"::xotcl::Object instproc init args {}\n"
"::xotcl::Object create ::xotcl::@\n"
"::xotcl::@ proc unknown args {}\n"
"namespace eval ::xotcl { namespace export @ }\n"
"foreach cmd {array append lappend trace eval} {\n"
"::xotcl::Object insttclcmd $cmd}\n"
"::xotcl::Object instproc self {} {return [::xotcl::self]}\n"
"::xotcl::Object instproc defaultmethod {} {\n"
"return [::xotcl::self]}\n"
"::xotcl::Object instproc filterappend f {\n"
"::xotcl::my filter [concat [::xotcl::my info filter -guards] $f]}\n"
"::xotcl::Object instproc mixinappend m {\n"
"::xotcl::my mixin [concat [::xotcl::my info mixin] $m]}\n"
"::xotcl::Class instproc instfilterappend f {\n"
"::xotcl::my instfilter [concat [::xotcl::my info instfilter -guards] $f]}\n"
"::xotcl::Class instproc instmixinappend m {\n"
"::xotcl::my instmixin [concat [::xotcl::my info instmixin] $m]}\n"
"::xotcl::Object instproc hasclass cl {\n"
"if {[::xotcl::my ismixin $cl]} {return 1}\n"
"::xotcl::my istype $cl}\n"
"::xotcl::Object proc unsetExitHandler {} {\n"
"::xotcl::Object proc __exitHandler {} {\n"
";}}\n"
"::xotcl::Object unsetExitHandler\n"
"::xotcl::Object proc setExitHandler {newbody} {\n"
"::xotcl::Object proc __exitHandler {} $newbody}\n"
"::xotcl::Object proc getExitHandler {} {\n"
"::xotcl::Object info body __exitHandler}\n"
"::xotcl::Class::Parameter superclass ::xotcl::Class\n"
"::xotcl::Class::Parameter instproc mkParameter {obj name args} {\n"
"if {[$obj exists $name]} {\n"
"eval [$obj set $name] configure $args} else {\n"
"$obj set $name [eval ::xotcl::my new -childof $obj $args]}}\n"
"::xotcl::Class::Parameter instproc getParameter {obj name args} {\n"
"[$obj set $name]}\n"
"::xotcl::Class::Parameter proc Class {param args} {\n"
"::xotcl::my set access [lindex $param 0]\n"
"::xotcl::my set setter mkParameter\n"
"::xotcl::my set getter getParameter\n"
"::xotcl::my set extra {[::xotcl::self]}\n"
"::xotcl::my set defaultParam [lrange $param 1 end]}\n"
"::xotcl::Class::Parameter proc default {val} {\n"
"[::xotcl::my set cl] set __defaults([::xotcl::my set name]) $val}\n"
"::xotcl::Class::Parameter proc setter x {\n"
"::xotcl::my set setter $x}\n"
"::xotcl::Class::Parameter proc getter x {\n"
"::xotcl::my set getter $x}\n"
"::xotcl::Class::Parameter proc access obj {\n"
"::xotcl::my set access $obj\n"
"::xotcl::my set extra \\[::xotcl::self\\]\n"
"foreach v [$obj info vars] {::xotcl::my set $v [$obj set $v]}}\n"
"::xotcl::Class::Parameter proc mkGetterSetter {cl name args} {\n"
"set l [llength $args]\n"
"if {$l == 0} {\n"
"$cl instparametercmd $name} elseif {$l == 1} {\n"
"$cl set __defaults($name) [lindex $args 0]\n"
"$cl instparametercmd $name} else {\n"
"::xotcl::my set name $name\n"
"::xotcl::my set cl $cl\n"
"::eval ::xotcl::my configure $args\n"
"if {[::xotcl::my exists extra] || [::xotcl::my exists setter] ||\n"
"[::xotcl::my exists getter] || [::xotcl::my exists access]} {\n"
"::xotcl::my instvar extra setter getter access defaultParam\n"
"if {![info exists extra]} {set extra \"\"}\n"
"if {![info exists defaultParam]} {set defaultParam \"\"}\n"
"if {![info exists setter]} {set setter set}\n"
"if {![info exists getter]} {set getter set}\n"
"if {![info exists access]} {set access ::xotcl::my}\n"
"$cl instproc $name args \"\n"
"if {\\[llength \\$args] == 0} {\n"
"return \\[$access $getter $extra $name\\]} else {\n"
"return \\[eval $access $setter $extra $name \\$args $defaultParam \\]}\"\n"
"foreach instvar {extra defaultParam setter getter access} {\n"
"if {[::xotcl::my exists $instvar]} {::xotcl::my unset $instvar}}} else {\n"
"$cl instparametercmd $name}}}\n"
"::xotcl::Class::Parameter proc values {param args} {\n"
"set cl [::xotcl::my set cl]\n"
"set ci [$cl info instinvar]\n"
"set valueTest {}\n"
"foreach a $args {\n"
"::lappend valueTest \"\\[\\$cl set $param\\] == [list $a]\"}\n"
"::lappend ci [join $valueTest \" || \"]\n"
"$cl instinvar $ci}\n"
"::xotcl::Object instproc abstract {methtype methname arglist} {\n"
"if {$methtype  != \"proc\" && $methtype != \"instproc\"} {\n"
"error \"invalid method type '$methtype', \\\n"
"must be either 'proc' or 'instproc'.\"}\n"
"::xotcl::my $methtype $methname $arglist \"\n"
"if {\\[::xotcl::self callingproc\\] != \\[::xotcl::self proc\\] &&\n"
"\\[::xotcl::self callingobject\\] != \\[::xotcl::self\\]} {\n"
"error \\\"Abstract method $methname $arglist called\\\"} else {::xotcl::next}\n"
"\"}\n"
"::xotcl::Class create ::xotcl::Object::CopyHandler -parameter {\n"
"{targetList \"\"}\n"
"{dest \"\"}\n"
"objLength}\n"
"::xotcl::Object::CopyHandler instproc makeTargetList t {\n"
"::xotcl::my lappend targetList $t\n"
"if {[::xotcl::my isobject $t]} {\n"
"if {[$t info hasNamespace]} {\n"
"set children [$t info children]} else {\n"
"return}}\n"
"foreach c [namespace children $t] {\n"
"if {![::xotcl::my isobject $c]} {\n"
"lappend children [namespace children $t]}}\n"
"foreach c $children {\n"
"::xotcl::my makeTargetList $c}}\n"
"::xotcl::Object::CopyHandler instproc copyNSVarsAndCmds {orig dest} {\n"
"::xotcl::namespace_copyvars $orig $dest\n"
"::xotcl::namespace_copycmds $orig $dest}\n"
"::xotcl::Object::CopyHandler instproc getDest origin {\n"
"set tail [string range $origin [::xotcl::my set objLength] end]\n"
"return ::[string trimleft [::xotcl::my set dest]$tail :]}\n"
"::xotcl::Object::CopyHandler instproc copyTargets {} {\n"
"foreach origin [::xotcl::my set targetList] {\n"
"set dest [::xotcl::my getDest $origin]\n"
"if {[::xotcl::my isobject $origin]} {\n"
"if {[::xotcl::my isclass $origin]} {\n"
"set cl [[$origin info class] create $dest -noinit]\n"
"set obj $cl\n"
"$cl superclass [$origin info superclass]\n"
"$cl parameterclass [$origin info parameterclass]\n"
"$cl parameter [$origin info parameter]\n"
"$cl instinvar [$origin info instinvar]\n"
"$cl instfilter [$origin info instfilter -guards]\n"
"$cl instmixin [$origin info instmixin]\n"
"my copyNSVarsAndCmds ::xotcl::classes::$origin ::xotcl::classes::$dest} else {\n"
"set obj [[$origin info class] create $dest -noinit]}\n"
"$obj invar [$origin info invar]\n"
"$obj check [$origin info check]\n"
"$obj mixin [$origin info mixin]\n"
"$obj filter [$origin info filter -guards]\n"
"if {[$origin info hasNamespace]} {\n"
"$obj requireNamespace}} else {\n"
"namespace eval $dest {}}\n"
"::xotcl::my copyNSVarsAndCmds $origin $dest}}\n"
"::xotcl::Object::CopyHandler instproc copy {obj dest} {\n"
"::xotcl::my set objLength [string length $obj]\n"
"::xotcl::my set dest $dest\n"
"::xotcl::my makeTargetList $obj\n"
"::xotcl::my copyTargets}\n"
"::xotcl::Object instproc copy newName {\n"
"if {[string compare [string trimleft $newName :] [string trimleft [::xotcl::self] :]]} {\n"
"[[::xotcl::self class]::CopyHandler new -volatile] copy [::xotcl::self] $newName}}\n"
"::xotcl::Object instproc move newName {\n"
"if {[string compare [string trimleft $newName :] [string trimleft [::xotcl::self] :]]} {\n"
"if {$newName != \"\"} {\n"
"::xotcl::my copy $newName}\n"
"if {[::xotcl::my isclass [::xotcl::self]] && $newName != \"\"} {\n"
"foreach subclass [::xotcl::my info subclass] {\n"
"set scl [$subclass info superclass]\n"
"if {[set index [lsearch -exact $scl [::xotcl::self]]] != -1} {\n"
"set scl [lreplace $scl $index $index $newName]\n"
"$subclass superclass $scl}}	}\n"
"::xotcl::my destroy}}\n"
"::xotcl::Object create ::xotcl::config\n"
"::xotcl::config proc load {obj file} {\n"
"source $file\n"
"foreach i [array names ::auto_index [list $obj *proc *]] {\n"
"set type [lindex $i 1]\n"
"set meth [lindex $i 2]\n"
"if {[$obj info ${type}s $meth] == {}} {\n"
"$obj $type $meth auto $::auto_index($i)}}}\n"
"::xotcl::config proc mkindex {meta dir args} {\n"
"set sp {[ 	]+}\n"
"set st {^[ 	]*}\n"
"set wd {([^ 	;]+)}\n"
"foreach creator $meta {\n"
"::lappend cp $st$creator${sp}create$sp$wd\n"
"::lappend ap $st$creator$sp$wd}\n"
"foreach method {proc instproc} {\n"
"::lappend mp $st$wd${sp}($method)$sp$wd}\n"
"foreach cl [concat ::xotcl::Class [::xotcl::Class info heritage]] {\n"
"eval ::lappend meths [$cl info instcommands]}\n"
"set old [pwd]\n"
"cd $dir\n"
"::append idx \"# Tcl autoload index file, version 2.0\\n\"\n"
"::append idx \"# xotcl additions generated with \"\n"
"::append idx \"\\\"::xotcl::config::mkindex [list $meta] [list $dir] $args\\\"\\n\"\n"
"set oc 0\n"
"set mc 0\n"
"foreach file [eval glob -nocomplain -- $args] {\n"
"if {[catch {set f [open $file]} msg]} then {\n"
"catch {close $f}\n"
"cd $old\n"
"error $msg}\n"
"while {[gets $f line] >= 0} {\n"
"foreach c $cp {\n"
"if {[regexp $c $line x obj]==1 &&\n"
"[string index $obj 0]!={$}} then {\n"
"::incr oc\n"
"::append idx \"set auto_index($obj) \"\n"
"::append idx \"\\\"::xotcl::config::load $obj \\$dir/$file\\\"\\n\"}}\n"
"foreach a $ap {\n"
"if {[regexp $a $line x obj]==1 &&\n"
"[string index $obj 0]!={$} &&\n"
"[lsearch -exact $meths $obj]==-1} {\n"
"::incr oc\n"
"::append idx \"set auto_index($obj) \"\n"
"::append idx \"\\\"::xotcl::config::load $obj \\$dir/$file\\\"\\n\"}}\n"
"foreach m $mp {\n"
"if {[regexp $m $line x obj ty pr]==1 &&\n"
"[string index $obj 0]!={$} &&\n"
"[string index $pr 0]!={$}} then {\n"
"::incr mc\n"
"::append idx \"set \\{auto_index($obj \"\n"
"::append idx \"$ty $pr)\\} \\\"source \\$dir/$file\\\"\\n\"}}}\n"
"close $f}\n"
"set t [open tclIndex a+]\n"
"puts $t $idx nonewline\n"
"close $t\n"
"cd $old\n"
"return \"$oc objects, $mc methods\"}\n"
"xotcl::Object instproc extractConfigureArg {al name {cutTheArg 0}} {\n"
"set value \"\"\n"
"upvar $al argList\n"
"set largs [llength $argList]\n"
"for {set i 0} {$i < $largs} {incr i} {\n"
"if {[lindex $argList $i] == $name && $i + 1 < $largs} {\n"
"set startIndex $i\n"
"set endIndex [expr {$i + 1}]\n"
"while {$endIndex < $largs &&\n"
"[string first - [lindex $argList $endIndex]] != 0} {\n"
"lappend value [lindex $argList $endIndex]\n"
"incr endIndex}}}\n"
"if {[info exists startIndex] && $cutTheArg != 0} {\n"
"set argList [lreplace $argList $startIndex [expr {$endIndex - 1}]]}\n"
"return $value}\n"
"::xotcl::Object create ::xotcl::rcs\n"
"::xotcl::rcs proc date string {\n"
"lreplace [lreplace $string 0 0] end end}\n"
"::xotcl::rcs proc version string {\n"
"lindex $string 2}\n"
"set ::xotcl::confdir ~/.xotcl\n"
"set ::xotcl::logdir $::xotcl::confdir/log\n"
"::xotcl::Class proc __unknown name {}\n"
"";

