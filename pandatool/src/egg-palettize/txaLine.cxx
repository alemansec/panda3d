// Filename: txaLine.cxx
// Created by:  drose (30Nov00)
// 
////////////////////////////////////////////////////////////////////

#include "txaLine.h"
#include "string_utils.h"
#include "eggFile.h"
#include "palettizer.h"
#include "textureImage.h"
#include "sourceTextureImage.h"
#include "paletteGroup.h"

#include <notify.h>
#include <pnmFileType.h>

////////////////////////////////////////////////////////////////////
//     Function: TxaLine::Constructor
//       Access: Public
//  Description: 
////////////////////////////////////////////////////////////////////
TxaLine::
TxaLine() {
  _size_type = ST_none;
  _scale = 0.0;
  _x_size = 0;
  _y_size = 0;
  _num_channels = 0;
  _format = EggTexture::F_unspecified;
  _got_margin = false;
  _margin = 0;
  _got_repeat_threshold = false;
  _repeat_threshold = 0.0;
  _color_type = (PNMFileType *)NULL;
  _alpha_type = (PNMFileType *)NULL;
}

////////////////////////////////////////////////////////////////////
//     Function: TxaLine::parse
//       Access: Public
//  Description: Accepts a string that defines a line of the .txa file
//               and parses it into its constinuent parts.  Returns
//               true if successful, false on error.
////////////////////////////////////////////////////////////////////
bool TxaLine::
parse(const string &line) {
  size_t colon = line.find(':');
  if (colon == string::npos) {
    nout << "Colon required.\n";
  }

  // Chop up the first part of the string (preceding the colon) into
  // its individual words.  These are patterns to match.
  vector_string words;
  extract_words(line.substr(0, colon), words);

  vector_string::iterator wi;
  for (wi = words.begin(); wi != words.end(); ++wi) {
    _patterns.push_back(GlobPattern(*wi));
  }

  if (_patterns.empty()) {
    nout << "No texture or egg filenames given.\n";
    return false;
  }

  // Now chop up the rest of the string (following the colon) into its
  // individual words.  These are keywords and size indications.
  words.clear();
  extract_words(line.substr(colon + 1), words);

  wi = words.begin();
  while (wi != words.end()) {
    const string &word = *wi;
    nassertr(!word.empty(), false);

    if (isdigit(word[0])) {
      // This is either a new size or a scale percentage.
      if (_size_type != ST_none) {
	nout << "Invalid repeated size request: " << word << "\n";
	return false;
      }
      if (word[word.length() - 1] == '%') {
	// It's a scale percentage!
	_size_type = ST_scale;
	const char *nptr = word.c_str();
	char *endptr;
	_scale = strtod(nptr, &endptr);
	if (*endptr != '%') {
	  nout << "Invalid scale factor: " << word << "\n";
	  return false;
	}
	++wi;

      } else {
	// Collect a number of consecutive numeric fields.
	vector<int> numbers;
	while (wi != words.end() && isdigit((*wi)[0])) {
	  const string &word = *wi;
	  const char *nptr = word.c_str();
	  char *endptr;
	  numbers.push_back(strtol(nptr, &endptr, 0));
	  if (*endptr != '\0') {
	    nout << "Invalid size: " << word << "\n";
	  }
	  ++wi;
	}
	if (numbers.size() < 2) {
	  nout << "At least two size numbers must be given, or a percent sign used to indicate scaling.\n";
	  return false;

	} else if (numbers.size() == 2) {
	  _size_type = ST_explicit_2;
	  _x_size = numbers[0];
	  _y_size = numbers[1];

	} else if (numbers.size() == 3) {
	  _size_type = ST_explicit_3;
	  _x_size = numbers[0];
	  _y_size = numbers[1];
	  _num_channels = numbers[2];

	} else {
	  nout << "Too many size numbers given.\n";
	  return false;
	}
      }

    } else {
      // The word does not begin with a digit; therefore it's either a
      // keyword or an image file type request.
      if (word == "omit") {
	_keywords.push_back(KW_omit);

      } else if (word == "nearest") {
	_keywords.push_back(KW_nearest);

      } else if (word == "linear") {
	_keywords.push_back(KW_linear);

      } else if (word == "mipmap") {
	_keywords.push_back(KW_mipmap);

      } else if (word == "cont") {
	_keywords.push_back(KW_cont);

      } else if (word == "margin") {
	++wi;
	if (wi == words.end()) {
	  nout << "Argument required for 'margin'.\n";
	  return false;
	} 
	  
	const string &arg = (*wi);
	const char *nptr = arg.c_str();
	char *endptr;
	_margin = strtol(nptr, &endptr, 10);
	if (*endptr != '\0') {
	  nout << "Not an integer: " << arg << "\n";
	  return false;
	}
	if (_margin < 0) {
	  nout << "Invalid margin: " << _margin << "\n";
	  return false;
	}
	_got_margin = true;

      } else if (word == "repeat") {
	++wi;
	if (wi == words.end()) {
	  nout << "Argument required for 'repeat'.\n";
	  return false;
	}
	 
	const string &arg = (*wi);
	const char *nptr = arg.c_str();
	char *endptr;
	_repeat_threshold = strtod(nptr, &endptr);
	if (*endptr != '\0' && *endptr != '%') {
	  nout << "Not a number: " << arg << "\n";
	  return false;
	}
	if (_repeat_threshold < 0.0) {
	  nout << "Invalid repeat threshold: " << _repeat_threshold << "\n";
	  return false;
	}
	_got_repeat_threshold = true;

      } else {
	// Maybe it's a format name.
	EggTexture::Format format = EggTexture::string_format(word);
	if (format != EggTexture::F_unspecified) {
	  _format = format;
	} else {
	  // Maybe it's a group name.
	  PaletteGroup *group = pal->test_palette_group(word);
	  if (group != (PaletteGroup *)NULL) {
	    _palette_groups.insert(group);
	    
	  } else {
	    // Maybe it's an image file request.
	    if (!parse_image_type_request(word, _color_type, _alpha_type)) {
	      return false;
	    }
	  }
	}
      }
      ++wi;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: TxaLine::match_egg
//       Access: Public
//  Description: Compares the patterns on the line to the indicated
//               EggFile.  If they match, updates the egg with the
//               appropriate information.  Returns true if a match is
//               detected and the search for another line should stop,
//               or false if a match is not detected (or if the
//               keyword "cont" is present, which means the search
//               should continue regardless).
////////////////////////////////////////////////////////////////////
bool TxaLine::
match_egg(EggFile *egg_file) const {
  string name = egg_file->get_name();

  bool matched_any = false;
  Patterns::const_iterator pi;
  for (pi = _patterns.begin(); 
       pi != _patterns.end() && !matched_any;
       ++pi) {
    matched_any = (*pi).matches(name);
  }

  if (!matched_any) {
    // No match this line; continue.
    return false;
  }

  bool got_cont = false;
  Keywords::const_iterator ki;
  for (ki = _keywords.begin(); ki != _keywords.end(); ++ki) {
    switch (*ki) {
    case KW_omit:
      break;

    case KW_nearest:
    case KW_linear:
    case KW_mipmap:
      // These mean nothing to an egg file.
      break;

    case KW_cont:
      got_cont = true;
      break;
    }
  }

  egg_file->_assigned_groups.make_union
    (egg_file->_assigned_groups, _palette_groups);

  if (got_cont) {
    // If we have the "cont" keyword, we should keep scanning for
    // another line, even though we matched this one.
    return false;
  }

  // Otherwise, in the normal case, a match ends the search for
  // matches.
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: TxaLine::match_texture
//       Access: Public
//  Description: Compares the patterns on the line to the indicated
//               TextureImage.  If they match, updates the texture
//               with the appropriate information.  Returns true if a
//               match is detected and the search for another line
//               should stop, or false if a match is not detected (or
//               if the keyword "cont" is present, which means the
//               search should continue regardless).
////////////////////////////////////////////////////////////////////
bool TxaLine::
match_texture(TextureImage *texture) const {
  string name = texture->get_name();

  bool matched_any = false;
  Patterns::const_iterator pi;
  for (pi = _patterns.begin(); 
       pi != _patterns.end() && !matched_any;
       ++pi) {
    matched_any = (*pi).matches(name);
  }

  if (!matched_any) {
    // No match this line; continue.
    return false;
  }

  SourceTextureImage *source = texture->get_preferred_source();
  TextureRequest &request = texture->_request;

  if (!request._got_size) {
    switch (_size_type) {
    case ST_none:
      break;

    case ST_scale:
      if (source->get_size()) {
	request._got_size = true;
	request._x_size = (int)(source->get_x_size() * _scale / 100.0);
	request._y_size = (int)(source->get_y_size() * _scale / 100.0);
      }
      break;

    case ST_explicit_3:
      request._got_num_channels = true;
      request._num_channels = _num_channels;
      // fall through

    case ST_explicit_2:
      request._got_size = true;
      request._x_size = _x_size;
      request._y_size = _y_size;
      break;
    }
  }

  if (_got_margin) {
    request._margin = _margin;
  }

  if (_got_repeat_threshold) {
    request._repeat_threshold = _repeat_threshold;
  }
  
  if (_color_type != (PNMFileType *)NULL) {
    request._properties._color_type = _color_type;
    request._properties._alpha_type = _alpha_type;
  }

  if (_format != EggTexture::F_unspecified) {
    request._format = _format;
  }
  
  bool got_cont = false;
  Keywords::const_iterator ki;
  for (ki = _keywords.begin(); ki != _keywords.end(); ++ki) {
    switch (*ki) {
    case KW_omit:
      request._omit = true;
      break;

    case KW_nearest:
      request._minfilter = EggTexture::FT_nearest;
      request._magfilter = EggTexture::FT_nearest;
      break;

    case KW_linear:
      request._minfilter = EggTexture::FT_linear;
      request._magfilter = EggTexture::FT_linear;
      break;

    case KW_mipmap:
      request._minfilter = EggTexture::FT_linear_mipmap_linear;
      request._magfilter = EggTexture::FT_linear_mipmap_linear;
      break;

    case KW_cont:
      got_cont = true;
      break;
    }
  }

  texture->_explicitly_assigned_groups.make_union
    (texture->_explicitly_assigned_groups, _palette_groups);

  if (got_cont) {
    // If we have the "cont" keyword, we should keep scanning for
    // another line, even though we matched this one.
    return false;
  }

  // Otherwise, in the normal case, a match ends the search for
  // matches.
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: TxaLine::output
//       Access: Public
//  Description: 
////////////////////////////////////////////////////////////////////
void TxaLine::
output(ostream &out) const {
  Patterns::const_iterator pi;
  for (pi = _patterns.begin(); pi != _patterns.end(); ++pi) {
    out << (*pi) << " ";
  }
  out << ":";

  switch (_size_type) {
  case ST_none:
    break;

  case ST_scale:
    out << " " << _scale << "%";
    break;

  case ST_explicit_2:
    out << " " << _x_size << " " << _y_size;
    break;

  case ST_explicit_3:
    out << " " << _x_size << " " << _y_size << " " << _num_channels;
    break;
  }

  if (_got_margin) {
    out << " margin " << _margin;
  }

  if (_got_repeat_threshold) {
    out << " repeat " << _repeat_threshold;
  }

  Keywords::const_iterator ki;
  for (ki = _keywords.begin(); ki != _keywords.end(); ++ki) {
    switch (*ki) {
    case KW_omit:
      out << " omit";
      break;

    case KW_nearest:
      out << " nearest";
      break;

    case KW_linear:
      out << " linear";
      break;

    case KW_mipmap:
      out << " mipmap";
      break;

    case KW_cont:
      out << " cont";
      break;
    }
  }

  PaletteGroups::const_iterator gi;
  for (gi = _palette_groups.begin(); gi != _palette_groups.end(); ++gi) {
    out << " " << (*gi)->get_name();
  }

  if (_color_type != (PNMFileType *)NULL) {
    out << " " << _color_type->get_suggested_extension();
    if (_alpha_type != (PNMFileType *)NULL) {
      out << "," << _alpha_type->get_suggested_extension();
    }
  }
}
