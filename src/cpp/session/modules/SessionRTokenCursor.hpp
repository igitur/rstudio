/*
 * RTokenCursor.hpp
 *
 * Copyright (C) 2009-2015 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */

#ifndef SESSION_MODULES_RTOKENCURSOR_HPP
#define SESSION_MODULES_RTOKENCURSOR_HPP

#include <core/r_util/RTokenizer.hpp>
#include "SessionRParser.hpp"

#include <boost/function.hpp>

#include <core/collection/Position.hpp>

#include <core/Macros.hpp>

namespace rstudio {
namespace session {
namespace modules {
namespace token_cursor {

using namespace rparser;
using namespace core::collection;
using namespace core::r_util;
using namespace core::r_util::token_utils;

// NOTE: TokenCursors store a reference to the set of tokens
// they use, so they are only valid as long as the underlying
// tokens are valid.
class RTokenCursor
{
private:
   
   RTokenCursor(const core::r_util::RTokens &rTokens,
               std::size_t offset,
               std::size_t n)
      : rTokens_(rTokens),
        offset_(offset),
        n_(n)
   {}
   
public:
   
   explicit RTokenCursor(const core::r_util::RTokens& rTokens)
      : rTokens_(rTokens), offset_(0), n_(rTokens.size()) {}
   
   RTokenCursor(const core::r_util::RTokens &rTokens,
               std::size_t offset)
      : rTokens_(rTokens), offset_(offset), n_(rTokens.size()) {}
   
   RTokenCursor clone() const
   {
      return RTokenCursor(rTokens_, offset_, n_);
   }
   
   const core::r_util::RTokens& tokens() const
   {
      return rTokens_;
   }
   
   bool moveToNextToken()
   {
      if (UNLIKELY(offset_ == n_ - 1))
         return false;
      
      ++offset_;
      return true;
   }
   
   bool moveToPreviousToken()
   {
      if (UNLIKELY(offset_ == 0))
         return false;
      
      --offset_;
      return true;
   }
   
   const RToken& currentToken() const
   {
      return rTokens_.at(offset_);
   }
   
   const Position currentPosition() const
   {
      const RToken& token = currentToken();
      return Position(token.row(), token.column());
   }
   
   std::wstring::const_iterator begin() const
   {
      return currentToken().begin();
   }
   
   std::wstring::const_iterator end() const
   {
      return currentToken().end();
   }
   
   const RToken& nextToken() const
   {
      return rTokens_.at(offset_ + 1);
   }
   
   const RToken& previousToken() const
   {
      return rTokens_.at(offset_ - 1);
   }
   
   const RToken& nextSignificantToken(std::size_t times = 1) const
   {
      int offset = 0;
      while (times != 0)
      {
         ++offset;
         while (isWhitespaceOrComment(rTokens_.at(offset_ + offset)))
            ++offset;
         
         --times;
      }
      
      return rTokens_.at(offset_ + offset);
   }
   
   const RToken& previousSignificantToken(std::size_t times = 1) const
   {
      int offset = 0;
      while (times != 0)
      {
         ++offset;
         while (isWhitespaceOrComment(rTokens_.at(offset_ - offset)))
            ++offset;
         
         --times;
      }
      
      return rTokens_.at(offset_ - offset);
   }
   
   operator const RToken&() const
   {
      return rTokens_.at(offset_);
   }
   
   operator ParseItem() const
   {
      return ParseItem(contentAsUtf8(),
                       currentPosition(),
                       NULL);
   }
   
   std::wstring content() const
   {
      return currentToken().content();
   }
   
   std::string contentAsUtf8() const
   {
      return currentToken().contentAsUtf8();
   }
   
   bool moveToNextSignificantToken()
   {
      if (!moveToNextToken())
         return false;
      
      if (!fwdOverWhitespaceAndComments())
         return false;
      
      return true;
   }
   
   bool moveToPreviousSignificantToken()
   {
      if (!moveToPreviousToken())
         return false;
      
      if (!bwdOverWhitespace())
         return false;
      
      return true;
   }
   
   bool contentEquals(const std::wstring& content) const
   {
      return currentToken().contentEquals(content);
   }
   
   RToken::TokenType type() const
   {
      return currentToken().type();
   }
   
   bool isType(RToken::TokenType type) const
   {
      return currentToken().isType(type);
   }

   bool contentContains(wchar_t character) const
   {
      return currentToken().contentContains(character);
   }
   
   bool fwdOverWhitespace()
   {
      while (currentToken().isType(RToken::WHITESPACE))
         if (!moveToNextToken())
            return false;
      return true;
   }
   
   bool bwdOverWhitespace()
   {
      while (currentToken().isType(RToken::WHITESPACE))
         if (!moveToPreviousToken())
            return false;
      return true;
   }
   
   bool fwdOverWhitespaceAndComments()
   {
      while (currentToken().isType(RToken::WHITESPACE) ||
             currentToken().isType(RToken::COMMENT))
         if (!moveToNextToken())
            return false;
      return true;
   }
   
   bool bwdOverWhitespaceAndComments()
   {
      while (currentToken().isType(RToken::WHITESPACE) ||
             currentToken().isType(RToken::COMMENT))
         if (!moveToPreviousToken())
            return false;
      return true;
   }
   
   // Move over whitespace tokens (that do not contain newlines)
   bool fwdOverBlank()
   {
      while (currentToken().isType(RToken::WHITESPACE) &&
             !currentToken().contentContains(L'\n'))
         if (!moveToNextToken())
            return false;
      return true;
   }
   
   bool bwdOverBlank()
   {
      while (currentToken().isType(RToken::WHITESPACE) &&
             !currentToken().contentContains(L'\n'))
         if (!moveToPreviousToken())
            return false;
      return true;
   }
   
   // We are at the end of the document if there are no
   // more significant tokens following.
   bool isAtEndOfDocument()
   {
      if (offset_ == n_ - 1)
         return true;
      
      RTokenCursor cursor = clone();
      ++cursor.offset_;
      
      if (!isWhitespaceOrComment(cursor))
         return false;
      
      cursor.fwdOverWhitespaceAndComments();
      if (isWhitespaceOrComment(cursor) && cursor.offset_ == n_ - 1)
         return true;
      
      return false;
   }
   
   bool finishesExpression()
   {
      const RToken& token = currentToken();
      bool isSemi = token.isType(RToken::SEMI);
      bool isComma = token.isType(RToken::COMMA);
      bool hasNewline = token.isType(RToken::WHITESPACE) &&
            token.contentContains(L'\n');
      bool isRightParen = isRightBracket(token);
      bool isFinalToken = offset_ == n_ - 1;

      return isSemi || hasNewline || isFinalToken || isRightParen || isComma;
   }
   
   friend std::ostream& operator <<(std::ostream& os,
                                    const RTokenCursor& cursor)
   {
      return os << cursor.currentToken().asString();
   }
   
   std::size_t row() const { return currentToken().row(); }
   std::size_t column() const { return currentToken().column(); }
   
private:
   
   bool doFwdToMatchingToken(RToken::TokenType leftTokenType,
                             RToken::TokenType rightTokenType)
   {
      if (!isType(leftTokenType))
         return false;
      
      RTokenCursor cursor = clone();
      int stack = 1;
      
      while (cursor.moveToNextToken())
      {
         stack += cursor.isType(leftTokenType);
         stack -= cursor.isType(rightTokenType);
         
         if (stack == 0)
         {
            offset_ = cursor.offset_;
            return true;
         }
      }
      
      return false;
   }
   
   bool doBwdToMatchingToken(RToken::TokenType leftTokenType,
                             RToken::TokenType rightTokenType)
   {
      if (!isType(rightTokenType))
         return false;
      
      RTokenCursor cursor = clone();
      int stack = 1;
      
      while (cursor.moveToPreviousToken())
      {
         stack += cursor.isType(rightTokenType) ? 1 : 0;
         stack -= cursor.isType(leftTokenType) ? 1 : 0;
         
         if (stack == 0)
         {
            offset_ = cursor.offset_;
            return true;
         }
      }
      return false;
   }
   
   static std::map<RToken::TokenType, RToken::TokenType> makeComplementMap()
   {
      std::map<RToken::TokenType, RToken::TokenType> map;

#define RSTUDIO_ADD_COMPLEMENT_2(__MAP__, __X__, __Y__)                        \
   do                                                                          \
   {                                                                           \
      __MAP__[__X__] = __Y__;                                                  \
      __MAP__[__Y__] = __X__;                                                  \
   } while (0)

#define RSTUDIO_ADD_COMPLEMENT(__MAP__, __BRACKET__)                                    \
   RSTUDIO_ADD_COMPLEMENT_2(__MAP__, RToken::L##__BRACKET__, RToken::R##__BRACKET__)

      RSTUDIO_ADD_COMPLEMENT(map, PAREN);
      RSTUDIO_ADD_COMPLEMENT(map, BRACKET);
      RSTUDIO_ADD_COMPLEMENT(map, BRACE);
      RSTUDIO_ADD_COMPLEMENT(map, DBRACKET);

#undef RSTUDIO_ADD_COMPLEMENT_2
#undef RSTUDIO_ADD_COMPLEMENT

      return map;
   }
   
   static std::map<RToken::TokenType, RToken::TokenType> complements()
   {
      static std::map<RToken::TokenType, RToken::TokenType> map = 
            makeComplementMap();
      
      return map;
   }

public:
  bool fwdToMatchingToken()
  {
     return doFwdToMatchingToken(type(), complements()[type()]);
  }

  bool bwdToMatchingToken()
  {
     return doBwdToMatchingToken(type(), complements()[type()]);
  }

public:
  
  bool isAtEndOfExpression() const
  {
     return nextToken().contentContains(L'\n') ||
            nextSignificantToken().isType(RToken::SEMI) ||
            nextSignificantToken().isType(RToken::COMMA);
  }
  
  bool endsExpression() const
  {
     return contentContains(L'\n') ||
            isType(RToken::SEMI) ||
            isType(RToken::COMMA);
  }
  
  bool isAtEndOfStatement(const ParseStatus& status)
  {
     // Whether we're in a parenthetical scope is important!
     // For example, these parse the same:
     //
     //     (foo\n(1))
     //     (foo  (1))
     //
     // while these parse differently:
     //
     //      foo\n(1)
     //      foo  (1)
     //
     if (!status.isInParentheticalScope() &&
         isWhitespaceWithNewline(nextToken()))
     {
        return true;
     }
     
     const RToken& next = nextSignificantToken();
     return !(
              isBinaryOp(next) ||
              next.isType(RToken::LPAREN) ||
              next.isType(RToken::LBRACKET) ||
              next.isType(RToken::LDBRACKET));
  }
  
  bool appearsToBeBinaryOperator() const
  {
     return isBinaryOp(currentToken()) &&
            isValidAsIdentifier(previousSignificantToken()) &&
            isValidAsIdentifier(nextSignificantToken());
  }
  
  // Move to the start of an 'evaluation' (from the end of a statement), e.g.
  //
  //    x$foo[[1]]$bar(1, 2, 3)$baz
  //
  // Note that we don't move to the start of a _statement_; e.g., we don't
  // walk over all binary operators. For example:
  //
  //    foo + x$foo$bar(1)
  //          ^   <--    ^
  //
  // We only move over extraction operators.
  bool moveToStartOfEvaluation()
  {
     RTokenCursor cursor = clone();
     while (true)
     {
        if (isRightBracket(cursor))
        {
           if (!cursor.bwdToMatchingToken())
              return false;
           
           if (!cursor.moveToPreviousSignificantToken())
              return false;
           
           continue;
        }
        
        if (isExtractionOperator(cursor.previousSignificantToken()))
        {
           if (!cursor.moveToPreviousSignificantToken())
              return false;
           
           if (!cursor.moveToPreviousSignificantToken())
              return false;
           
           continue;
        }
        
        break;
     }
     
     offset_ = cursor.offset_;
     return true;
  }
  
  // Get the 'evaluation' associated with a function call, e.g.
  //
  //    foo + bar::baz$bam()
  //          ^^^^^^^^^^^^
  //
  std::wstring getCallingString()
  {
     RTokenCursor cursor = clone();
     
     if (isLeftBracket(cursor))
        if (!cursor.moveToPreviousSignificantToken())
           return std::wstring();
     
     std::wstring::const_iterator end = cursor.end();
     if (!cursor.moveToStartOfEvaluation())
        return std::wstring();
     
     std::wstring::const_iterator begin = cursor.begin();
     return std::wstring(begin, end);
  }
  
  // Move to the end of an R statement, e.g.
  //
  //    x <- x + I(a, b, c)
  //         ^~~~~~~>~~~~~^
  //
  // 'inParens' is necessary because we need to know whether
  // newlines are significant or not (they are not significant within
  // a parenthetical scope). Returns 'true' if we reached the end of
  // a statement.
  bool moveToEndOfStatement(bool inParens)
  {
     while (true)
     {
        // When we're in a parenthetical statement, newlines are no
        // longer significant. This means that, for example,
        //
        //    (someFunction
        //     (1, 2, 3))
        //
        // is actually a function call to `someFunction(1, 2, 3)`, while
        //
        //    someFunction
        //    (1, 2, 3)
        //
        // is actually two separate statements (the second being invalid)
        if (!inParens && nextToken().contentContains(L'\n'))
           return true;
        
        // Bail on semi-colons.
        if (isType(RToken::SEMI))
           return true;
        
        // Move over unary operators
        while (isValidAsUnaryOperator(*this))
           if (!moveToNextSignificantToken())
              return false;
        
        // Walk over binary operator pairs.
        //
        // This branch takes us as follows:
        //
        //    a + b
        //    ^->-^
        //
        while (isBinaryOp(nextSignificantToken()))
        {
           if (!moveToNextSignificantToken())
              return false;
           
           if (!moveToNextSignificantToken())
              return false;
           
           continue;
        }
        
        // Check for a parenthetical statement and move over it.
        //
        //    a + (...)
        //        ^~~~^
        //
        if (isLeftBracket(*this))
        {
           if (!fwdToMatchingToken())
              return false;
           
           if (!inParens && nextToken().contentContains(L'\n'))
              return true;
           
           // Bail on semi-colons.
           if (isType(RToken::SEMI))
              return true;
           
           continue;
        }
        
        // Check for a function call and move over it.
        //
        //    foo::bar(...)
        //         ^~~~>~~^
        if (isLeftBracket(nextSignificantToken()))
        {
           if (!moveToNextSignificantToken())
              return false;
           
           if (!fwdToMatchingToken())
              return false;
           
           if (!inParens && nextToken().contentContains(L'\n'))
              return true;
           
           continue;
        }
        
        return true;
     }
  }
  
  std::size_t length() const { return currentToken().length(); }
  
private:
   
   const core::r_util::RTokens& rTokens_;
   std::size_t offset_;
   std::size_t n_;
};

} // namespace token_utils
} // namespace r_util
} // namespace core
} // namespace rstudio

#endif // SESSION_MODULES_RTOKENCURSOR_HPP
